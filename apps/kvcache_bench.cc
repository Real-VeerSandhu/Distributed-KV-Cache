#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "coordinator/coordinator_service.h"
#include "coordinator/global_index.h"
#include "coordinator/inproc_coordinator_client.h"
#include "coordinator/stale_reference_cleaner.h"
#include "coordinator/worker_directory.h"
#include "core/block_candidate.h"
#include "core/cache_key.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "core/ids.h"
#include "core/local_cache.h"
#include "core/span.h"
#include "pipeline/admission_stage.h"
#include "pipeline/lookup_stage.h"
#include "pipeline/remote_block_fetcher.h"
#include "pipeline/remote_resolution_stage.h"
#include "pipeline/request_pipeline.h"
#include "pipeline/route_types.h"
#include "policy/policies/always_admit.h"
#include "policy/policies/constant_latency.h"
#include "policy/policies/first_replica.h"
#include "policy/policies/hash_routing.h"
#include "policy/policies/lru_eviction.h"
#include "policy/policies/prefix_aware_eviction.h"
#include "policy/policies/random_routing.h"
#include "sim/event_sink.h"
#include "transport/inproc_transport.h"
#include "worker/worker_service.h"

using namespace kvcache;
using namespace kvcache::coordinator;
using namespace kvcache::pipeline;
using namespace kvcache::transport;
using namespace kvcache::worker;
using namespace kvcache::sim;
using namespace kvcache::policy;

namespace {

// ── Configuration ─────────────────────────────────────────────────────────────

struct BenchArgs {
    std::string routing = "hash_first_block";
    std::string eviction = "lru";
    std::string workload = "shared_system_prompt";
    uint32_t num_workers = 2;
    uint32_t capacity_per_worker = 12;
    uint32_t num_requests = 200;
    uint32_t seed = 42;
    std::string output_json;
    bool stream_events = false;
};

BenchArgs parseArgs(int argc, char** argv) {
    BenchArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--stream-events") {
            args.stream_events = true;
            continue;
        }
        if (i + 1 >= argc) continue;
        std::string value = argv[i + 1];
        if      (flag == "--routing")   { args.routing = value;   ++i; }
        else if (flag == "--eviction")  { args.eviction = value;  ++i; }
        else if (flag == "--workload")  { args.workload = value;  ++i; }
        else if (flag == "--workers")   { args.num_workers = static_cast<uint32_t>(std::stoul(value)); ++i; }
        else if (flag == "--capacity")  { args.capacity_per_worker = static_cast<uint32_t>(std::stoul(value)); ++i; }
        else if (flag == "--requests")  { args.num_requests = static_cast<uint32_t>(std::stoul(value)); ++i; }
        else if (flag == "--seed")      { args.seed = static_cast<uint32_t>(std::stoul(value)); ++i; }
        else if (flag == "--output-json") { args.output_json = value; ++i; }
    }
    return args;
}

// ── Workload Generation ────────────────────────────────────────────────────────

constexpr uint32_t BLOCK_SIZE = 16;
constexpr uint32_t PREFIX_BLOCKS = 4;
constexpr uint32_t SUFFIX_BLOCKS = 1;
constexpr uint32_t FAMILY_PREFIX_BLOCKS = 3;
constexpr uint32_t NUM_FAMILIES = 4;
constexpr uint32_t CHURN_BLOCKS = 6;

struct BenchRequest {
    uint32_t request_id;
    std::vector<TokenId> tokens;
    uint32_t family_id{0};
};

CacheKeyContext makeCtx() {
    return CacheKeyContext{ModelId{1}, TokenizerId{1}, BLOCK_SIZE, DType::BF16,
                           32, 8, 128, 0, 0};
}

std::vector<BenchRequest> generateSharedSystemPrompt(const BenchArgs& args) {
    std::mt19937 rng(args.seed);
    std::uniform_int_distribution<int32_t> suffix_dist(10000, 99999);

    const uint32_t prefix_tokens = PREFIX_BLOCKS * BLOCK_SIZE;
    const uint32_t suffix_tokens = SUFFIX_BLOCKS * BLOCK_SIZE;

    std::vector<BenchRequest> requests;
    requests.reserve(args.num_requests);

    for (uint32_t req = 0; req < args.num_requests; ++req) {
        std::vector<TokenId> tokens;
        tokens.reserve(prefix_tokens + suffix_tokens);
        for (uint32_t t = 1; t <= prefix_tokens; ++t) tokens.push_back(static_cast<TokenId>(t));
        for (uint32_t t = 0; t < suffix_tokens; ++t) tokens.push_back(suffix_dist(rng));
        requests.push_back({req, std::move(tokens), 0});
    }
    return requests;
}

std::vector<BenchRequest> generateFewShotFamilies(const BenchArgs& args) {
    std::mt19937 rng(args.seed);
    std::uniform_int_distribution<int32_t> suffix_dist(10000, 99999);

    const uint32_t prefix_tokens = FAMILY_PREFIX_BLOCKS * BLOCK_SIZE;
    const uint32_t suffix_tokens = SUFFIX_BLOCKS * BLOCK_SIZE;

    std::vector<BenchRequest> requests;
    requests.reserve(args.num_requests);

    for (uint32_t req = 0; req < args.num_requests; ++req) {
        const uint32_t family = req % NUM_FAMILIES;
        const uint32_t base = (family + 1) * 1000;

        std::vector<TokenId> tokens;
        tokens.reserve(prefix_tokens + suffix_tokens);
        for (uint32_t t = 0; t < prefix_tokens; ++t)
            tokens.push_back(static_cast<TokenId>(base + t));
        for (uint32_t t = 0; t < suffix_tokens; ++t)
            tokens.push_back(suffix_dist(rng));

        requests.push_back({req, std::move(tokens), family});
    }
    return requests;
}

std::vector<BenchRequest> generateAdversarialChurn(const BenchArgs& args) {
    std::mt19937 rng(args.seed);
    std::uniform_int_distribution<int32_t> dist(1, 999999);

    const uint32_t total_tokens = CHURN_BLOCKS * BLOCK_SIZE;

    std::vector<BenchRequest> requests;
    requests.reserve(args.num_requests);

    for (uint32_t req = 0; req < args.num_requests; ++req) {
        std::vector<TokenId> tokens;
        tokens.reserve(total_tokens);
        for (uint32_t t = 0; t < total_tokens; ++t) tokens.push_back(dist(rng));
        requests.push_back({req, std::move(tokens), 0});
    }
    return requests;
}

std::vector<BenchRequest> generateWorkload(const BenchArgs& args) {
    if (args.workload == "few_shot_families")  return generateFewShotFamilies(args);
    if (args.workload == "adversarial_churn")  return generateAdversarialChurn(args);
    return generateSharedSystemPrompt(args);
}

// ── Event Sink ────────────────────────────────────────────────────────────────

class CountingTeeEventSink : public EventSink {
public:
    explicit CountingTeeEventSink(EventSink* downstream = nullptr) noexcept
        : downstream_(downstream) {}

    void record(const Event& event) override {
        if (downstream_) downstream_->record(event);
        if (std::holds_alternative<BlockAdmitted>(event)) ++admissions_;
        if (std::holds_alternative<BlockEvicted>(event))  ++evictions_;
    }

    [[nodiscard]] uint32_t admissions() const noexcept { return admissions_; }
    [[nodiscard]] uint32_t evictions()  const noexcept { return evictions_; }

private:
    EventSink* downstream_;
    uint32_t admissions_ = 0;
    uint32_t evictions_  = 0;
};

// ── Worker Rig ────────────────────────────────────────────────────────────────

struct WorkerRig {
    NodeId node_id;
    FakeClock clock;
    AlwaysAdmitPolicy admit_policy;
    std::unique_ptr<EvictionPolicy> evict_policy_owned;
    CountingTeeEventSink events;
    NullDecisionLogger decisions;
    LocalCache cache;
    ConstantLatencyModel latency;
    InprocTransport transport;
    FirstReplicaPolicy replica_policy;
    RemoteBlockFetcher fetcher;
    LookupStage lookup;
    AdmissionStage admission;
    uint32_t local_hits  = 0;
    uint32_t remote_hits = 0;

    WorkerRig(NodeId nid, uint32_t capacity, uint32_t block_size,
              std::unique_ptr<EvictionPolicy> ep, EventSink* stream_sink)
        : node_id(nid),
          evict_policy_owned(std::move(ep)),
          events(stream_sink),
          cache(capacity, block_size, clock, admit_policy, *evict_policy_owned, events, decisions),
          latency(0, 0, 0, 0),
          fetcher(transport, latency, events),
          lookup(cache),
          admission(cache) {}
};

// ── Factories ─────────────────────────────────────────────────────────────────

std::unique_ptr<EvictionPolicy> makeEvictionPolicy(const std::string& name) {
    if (name == "prefix_aware") return std::make_unique<PrefixAwareEvictionPolicy>();
    return std::make_unique<LruEvictionPolicy>();
}

std::unique_ptr<RoutingPolicy> makeRoutingPolicy(const BenchArgs& args) {
    if (args.routing == "hash_first_block") return std::make_unique<HashFirstBlockRoutingPolicy>();
    return std::make_unique<RandomRoutingPolicy>(args.seed);
}

// ── Block Candidate Helper ────────────────────────────────────────────────────

BlockCandidate makeCandidate(const CacheKeyContext& ctx, uint32_t block_idx,
                             const std::vector<TokenId>& full_tokens) {
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    const ContentHash h = computeContentHash(
        ctx_hash, block_idx, static_cast<uint16_t>(BLOCK_SIZE),
        Span<const TokenId>{full_tokens.data() + block_idx * BLOCK_SIZE, BLOCK_SIZE});
    std::vector<TokenId> toks(
        full_tokens.begin(),
        full_tokens.begin() + static_cast<ptrdiff_t>((block_idx + 1) * BLOCK_SIZE));
    return BlockCandidate{ctx, block_idx, std::move(toks), h, {}, BlockOrigin::LocallyComputed,
                          std::nullopt};
}

// ── JSON Output ───────────────────────────────────────────────────────────────

void writeJsonOutput(const BenchArgs& args,
                     const std::vector<std::unique_ptr<WorkerRig>>& rigs,
                     uint32_t total_prefix_possible) {
    uint32_t total_local = 0, total_remote = 0, total_evictions = 0, total_admissions = 0;

    nlohmann::json workers_json = nlohmann::json::array();
    for (const auto& rig : rigs) {
        total_local      += rig->local_hits;
        total_remote     += rig->remote_hits;
        total_evictions  += rig->events.evictions();
        total_admissions += rig->events.admissions();
        workers_json.push_back({
            {"node_id",     static_cast<uint64_t>(rig->node_id)},
            {"local_hits",  rig->local_hits},
            {"remote_hits", rig->remote_hits},
            {"evictions",   rig->events.evictions()},
            {"admissions",  rig->events.admissions()},
            {"gpu_sim_used", 0},
            {"host_used",    0},
        });
    }

    nlohmann::json j;
    j["config"] = {
        {"routing",             args.routing},
        {"eviction",            args.eviction},
        {"workload",            args.workload},
        {"num_workers",         args.num_workers},
        {"capacity_per_worker", args.capacity_per_worker},
        {"num_requests",        args.num_requests},
        {"seed",                args.seed},
    };
    j["results"] = {
        {"local_hits",            total_local},
        {"remote_hits",           total_remote},
        {"total_prefix_possible", total_prefix_possible},
        {"evictions",             total_evictions},
        {"admissions",            total_admissions},
    };
    j["workers"] = workers_json;

    std::ofstream out(args.output_json);
    out << j.dump(2) << '\n';
}

// ── Main Bench ────────────────────────────────────────────────────────────────

void runBench(const BenchArgs& args) {
    if (args.stream_events) std::cout << std::unitbuf;

    std::unique_ptr<JsonlEventSink> stream_sink;
    if (args.stream_events) {
        stream_sink = std::make_unique<JsonlEventSink>(std::cout);
    }
    EventSink* raw_stream = stream_sink.get();

    GlobalIndex global_idx;
    WorkerDirectory dir;
    StaleReferenceCleaner cleaner{global_idx};
    CoordinatorService coord_svc{global_idx, dir, cleaner};

    std::vector<std::unique_ptr<WorkerRig>> rigs;
    for (uint32_t i = 0; i < args.num_workers; ++i) {
        rigs.push_back(std::make_unique<WorkerRig>(
            NodeId{i}, args.capacity_per_worker, BLOCK_SIZE,
            makeEvictionPolicy(args.eviction), raw_stream));
        coord_svc.nodeJoin({NodeId{i}, ""});
        dir.nodeJoin({NodeId{i}, ""});
    }

    std::vector<std::unique_ptr<InprocCoordinatorClient>> coord_clients;
    std::vector<std::unique_ptr<NullEventSink>> pipe_events;
    std::vector<std::unique_ptr<RemoteResolutionStage>> remote_stages;
    std::vector<std::unique_ptr<RequestPipeline>> pipelines;
    std::vector<std::unique_ptr<WorkerService>> services;

    for (uint32_t i = 0; i < args.num_workers; ++i) {
        coord_clients.push_back(std::make_unique<InprocCoordinatorClient>(coord_svc));
        pipe_events.push_back(std::make_unique<NullEventSink>());
        remote_stages.push_back(std::make_unique<RemoteResolutionStage>(
            *coord_clients.back(), rigs[i]->replica_policy, rigs[i]->fetcher,
            *pipe_events.back()));
        pipelines.push_back(std::make_unique<RequestPipeline>(
            rigs[i]->lookup, *remote_stages.back(), rigs[i]->admission,
            *pipe_events.back()));
        services.push_back(std::make_unique<WorkerService>(rigs[i]->cache, *pipelines.back()));
    }

    for (uint32_t i = 0; i < args.num_workers; ++i) {
        for (uint32_t j = 0; j < args.num_workers; ++j) {
            if (i != j) rigs[i]->transport.registerWorker(NodeId{j}, services[j].get());
        }
    }

    const auto ctx = makeCtx();
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    const auto requests = generateWorkload(args);
    const uint32_t total_blocks_per_request =
        static_cast<uint32_t>(requests.empty() ? 0 : requests[0].tokens.size() / BLOCK_SIZE);

    auto routing_policy = makeRoutingPolicy(args);
    std::vector<NodePolicyFeatures> node_features;
    for (uint32_t i = 0; i < args.num_workers; ++i)
        node_features.push_back({NodeId{i}, 0, 0, 0, 0, 0, 0, 0, 0});

    uint32_t total_prefix_possible = 0;

    for (const auto& req : requests) {
        const uint32_t prefix_blocks = total_blocks_per_request > 0 ? total_blocks_per_request - 1 : 0;
        total_prefix_possible += prefix_blocks;

        const ContentHash first_block_hash = computeContentHash(
            ctx_hash, 0, static_cast<uint16_t>(BLOCK_SIZE),
            Span<const TokenId>{req.tokens.data(), BLOCK_SIZE});

        RoutingContext rctx{ctx_hash, total_blocks_per_request, first_block_hash, 0};
        const NodeId chosen = routing_policy->chooseNode(
            rctx, Span<const NodePolicyFeatures>{node_features.data(), node_features.size()});
        const uint32_t widx = static_cast<uint32_t>(chosen);

        RouteRequest route_req{RequestId{req.request_id}, ctx, req.tokens};
        auto result = pipelines[widx]->run(route_req);

        rigs[widx]->local_hits  += result.local_hit_blocks;
        rigs[widx]->remote_hits += result.remote_hit_blocks;

        const uint32_t first_miss = static_cast<uint32_t>(result.reusable_blocks.size());
        for (uint32_t bi = first_miss; bi < total_blocks_per_request; ++bi) {
            auto cand = makeCandidate(ctx, bi, req.tokens);
            const ContentHash h = cand.hash;
            auto admit = rigs[widx]->cache.admitBlock(cand);
            if (admit.status == AdmitResult::Status::Admitted && admit.handle.has_value()) {
                const BlockId bid = admit.handle->id();
                const uint64_t gen = admit.handle->metadata().generation;
                admit.handle.reset();
                coord_svc.announce(h, {chosen, bid, gen});
            }
        }
    }

    if (!args.output_json.empty()) writeJsonOutput(args, rigs, total_prefix_possible);

    uint32_t total_local = 0, total_remote = 0;
    for (const auto& rig : rigs) {
        total_local  += rig->local_hits;
        total_remote += rig->remote_hits;
    }
    const uint32_t total_hits = total_local + total_remote;
    const double hit_rate = total_prefix_possible > 0
        ? static_cast<double>(total_hits) / static_cast<double>(total_prefix_possible)
        : 0.0;
    const double remote_frac = total_hits > 0
        ? static_cast<double>(total_remote) / static_cast<double>(total_hits)
        : 0.0;

    std::cerr << "routing=" << args.routing
              << "  eviction=" << args.eviction
              << "  workers=" << args.num_workers
              << "  requests=" << args.num_requests
              << "  hit_rate=" << std::fixed << std::setprecision(3) << hit_rate
              << "  local=" << total_local
              << "  remote=" << total_remote
              << "  remote_frac=" << std::setprecision(3) << remote_frac
              << '\n';
}

// ── Legacy Phase 2 Bench (preserved) ─────────────────────────────────────────

struct Phase2Result {
    std::string policy_name;
    uint32_t prefix_hits;
    uint32_t prefix_possible;
    uint32_t evictions;
    uint32_t admissions;
};

Phase2Result runSingleNodeBench(const std::string& policy_name, EvictionPolicy& ep) {
    constexpr uint32_t SHARED_PREFIX_BLOCKS = 4;
    constexpr uint32_t UNIQUE_BLOCKS = 1;
    constexpr uint32_t TOTAL_REQUESTS = 200;
    constexpr uint32_t CAPACITY = SHARED_PREFIX_BLOCKS + 8;
    constexpr uint32_t TOTAL_BLOCKS = SHARED_PREFIX_BLOCKS + UNIQUE_BLOCKS;
    constexpr uint32_t SHARED_TOKENS = SHARED_PREFIX_BLOCKS * BLOCK_SIZE;

    FakeClock clock;
    AlwaysAdmitPolicy ap;
    InMemoryEventSink events;
    NullDecisionLogger decisions;
    LocalCache cache{CAPACITY, BLOCK_SIZE, clock, ap, ep, events, decisions};
    const auto ctx = makeCtx();

    std::vector<TokenId> shared_prefix(SHARED_TOKENS);
    std::iota(shared_prefix.begin(), shared_prefix.end(), TokenId{1});

    Phase2Result result{};
    result.policy_name = policy_name;

    for (uint32_t req = 0; req < TOTAL_REQUESTS; ++req) {
        clock.advanceNs(1000);
        std::vector<TokenId> full = shared_prefix;
        for (uint32_t t = 0; t < BLOCK_SIZE; ++t)
            full.push_back(static_cast<TokenId>(10000 + req * BLOCK_SIZE + t));

        auto outcome = cache.lookupPrefix(ctx, Span<const TokenId>{full.data(), full.size()});
        result.prefix_hits += std::min(static_cast<uint32_t>(outcome.matched_blocks.size()),
                                       SHARED_PREFIX_BLOCKS);
        result.prefix_possible += SHARED_PREFIX_BLOCKS;

        const uint32_t first_miss = static_cast<uint32_t>(outcome.matched_blocks.size());
        for (uint32_t bi = first_miss; bi < TOTAL_BLOCKS; ++bi) {
            auto admit = cache.admitBlock(makeCandidate(ctx, bi, full));
            if (admit.status == AdmitResult::Status::Admitted) ++result.admissions;
        }
    }
    result.evictions = static_cast<uint32_t>(events.eventsOfType<BlockEvicted>().size());
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    const BenchArgs args = parseArgs(argc, argv);

    if (argc > 1) {
        runBench(args);
        return 0;
    }

    // No args: run the legacy comparison demo.
    std::cout << "kvcache_bench — legacy demo (pass --routing / --eviction to configure)\n\n";

    std::cout << "── Phase 2: single-node eviction policy comparison ──────────────────\n";
    std::cout << std::left << std::setw(20) << "policy"
              << "  prefix_hit_rate  evictions  admissions\n"
              << std::string(60, '-') << '\n';

    LruEvictionPolicy lru;
    PrefixAwareEvictionPolicy prefix_aware;

    auto printP2 = [](const Phase2Result& r) {
        const double hr = static_cast<double>(r.prefix_hits) / static_cast<double>(r.prefix_possible);
        std::cout << std::left << std::setw(20) << r.policy_name
                  << "  hit_rate=" << std::fixed << std::setprecision(3) << hr
                  << "  evictions=" << r.evictions
                  << "  admissions=" << r.admissions << '\n';
    };

    printP2(runSingleNodeBench("lru", lru));
    printP2(runSingleNodeBench("prefix_aware", prefix_aware));

    return 0;
}
