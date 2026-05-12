#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

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

constexpr uint32_t BLOCK_SIZE = 16;
constexpr uint32_t SHARED_PREFIX_BLOCKS = 4;
constexpr uint32_t SHARED_PREFIX_TOKENS = SHARED_PREFIX_BLOCKS * BLOCK_SIZE;
constexpr uint32_t UNIQUE_BLOCKS_PER_REQUEST = 1;
constexpr uint32_t TOTAL_REQUESTS = 200;
constexpr uint32_t WORKER_CAPACITY = SHARED_PREFIX_BLOCKS + 8;
constexpr uint32_t NUM_WORKERS = 2;

CacheKeyContext makeCtx() {
    return CacheKeyContext{ModelId{1}, TokenizerId{1}, BLOCK_SIZE, DType::BF16,
                           32,        8,               128,         0,
                           0};
}

std::vector<TokenId> makeSharedPrefix() {
    std::vector<TokenId> v(SHARED_PREFIX_TOKENS);
    std::iota(v.begin(), v.end(), TokenId{1});
    return v;
}

std::vector<TokenId> makeSuffix(uint32_t req_id) {
    std::vector<TokenId> v(BLOCK_SIZE * UNIQUE_BLOCKS_PER_REQUEST);
    for (uint32_t i = 0; i < v.size(); ++i) {
        v[i] = static_cast<TokenId>(10000 + req_id * BLOCK_SIZE + static_cast<TokenId>(i));
    }
    return v;
}

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

// ── Phase 2 single-node bench (preserved) ─────────────────────────────────────

struct Phase2Result {
    std::string policy_name;
    uint32_t prefix_hits;
    uint32_t prefix_possible;
    uint32_t evictions;
    uint32_t admissions;
};

Phase2Result runSingleNodeBench(const std::string& policy_name, EvictionPolicy& ep) {
    FakeClock clock;
    AlwaysAdmitPolicy ap;
    InMemoryEventSink events;
    NullDecisionLogger decisions;
    LocalCache cache{WORKER_CAPACITY, BLOCK_SIZE, clock, ap, ep, events, decisions};
    const auto ctx = makeCtx();
    const auto shared_prefix = makeSharedPrefix();
    const uint32_t total_blocks = SHARED_PREFIX_BLOCKS + UNIQUE_BLOCKS_PER_REQUEST;

    Phase2Result result{};
    result.policy_name = policy_name;

    for (uint32_t req = 0; req < TOTAL_REQUESTS; ++req) {
        clock.advanceNs(1000);
        std::vector<TokenId> full = shared_prefix;
        const auto suffix = makeSuffix(req);
        full.insert(full.end(), suffix.begin(), suffix.end());

        auto outcome = cache.lookupPrefix(ctx, Span<const TokenId>{full.data(), full.size()});
        result.prefix_hits += std::min(static_cast<uint32_t>(outcome.matched_blocks.size()),
                                       SHARED_PREFIX_BLOCKS);
        result.prefix_possible += SHARED_PREFIX_BLOCKS;

        const uint32_t first_miss = static_cast<uint32_t>(outcome.matched_blocks.size());
        for (uint32_t bi = first_miss; bi < total_blocks; ++bi) {
            auto admit = cache.admitBlock(makeCandidate(ctx, bi, full));
            if (admit.status == AdmitResult::Status::Admitted) ++result.admissions;
        }
    }
    result.evictions = static_cast<uint32_t>(events.eventsOfType<BlockEvicted>().size());
    return result;
}

// ── Phase 4 multi-worker bench ────────────────────────────────────────────────

struct WorkerRig {
    NodeId node_id;
    FakeClock clock;
    NullEventSink events;
    AlwaysAdmitPolicy admit_policy;
    LruEvictionPolicy evict_policy;
    NullDecisionLogger decisions;
    LocalCache cache;
    ConstantLatencyModel latency;
    InprocTransport transport;
    FirstReplicaPolicy replica_policy;
    RemoteBlockFetcher fetcher;
    LookupStage lookup;
    AdmissionStage admission;

    explicit WorkerRig(NodeId nid)
        : node_id(nid),
          cache(WORKER_CAPACITY, BLOCK_SIZE, clock, admit_policy, evict_policy, events,
                decisions),
          latency(0, 0, 0, 0),
          fetcher(transport, latency, events),
          lookup(cache),
          admission(cache) {}
};

struct MultiWorkerResult {
    std::string routing_name;
    uint32_t local_hits;
    uint32_t remote_hits;
    uint32_t total_requests;
    uint32_t total_prefix_possible;
};

MultiWorkerResult runMultiWorkerBench(const std::string& routing_name,
                                       RoutingPolicy& routing_policy) {
    GlobalIndex global_idx;
    WorkerDirectory dir;
    StaleReferenceCleaner cleaner{global_idx};
    CoordinatorService coord_svc{global_idx, dir, cleaner};

    std::vector<std::unique_ptr<WorkerRig>> rigs;
    for (uint32_t i = 0; i < NUM_WORKERS; ++i) {
        rigs.push_back(std::make_unique<WorkerRig>(NodeId{i}));
        coord_svc.nodeJoin({NodeId{i}, ""});
        dir.nodeJoin({NodeId{i}, ""});
    }

    // Wire cross-transport: each worker can fetch from all others.
    std::vector<std::unique_ptr<InprocCoordinatorClient>> coord_clients;
    std::vector<std::unique_ptr<RemoteResolutionStage>> remote_stages;
    std::vector<std::unique_ptr<NullEventSink>> pipe_events;
    std::vector<std::unique_ptr<RequestPipeline>> pipelines;
    std::vector<std::unique_ptr<WorkerService>> services;

    for (uint32_t i = 0; i < NUM_WORKERS; ++i) {
        coord_clients.push_back(std::make_unique<InprocCoordinatorClient>(coord_svc));
        pipe_events.push_back(std::make_unique<NullEventSink>());
        auto* pe = pipe_events.back().get();
        remote_stages.push_back(std::make_unique<RemoteResolutionStage>(
            *coord_clients.back(), rigs[i]->replica_policy, rigs[i]->fetcher, *pe));
        pipelines.push_back(std::make_unique<RequestPipeline>(
            rigs[i]->lookup, *remote_stages.back(), rigs[i]->admission, *pe));
        services.push_back(
            std::make_unique<WorkerService>(rigs[i]->cache, *pipelines.back()));
    }

    // Register all workers with each transport.
    for (uint32_t i = 0; i < NUM_WORKERS; ++i) {
        for (uint32_t j = 0; j < NUM_WORKERS; ++j) {
            if (i != j) rigs[i]->transport.registerWorker(NodeId{j}, services[j].get());
        }
    }

    const auto ctx = makeCtx();
    const auto shared_prefix = makeSharedPrefix();
    const uint32_t total_blocks = SHARED_PREFIX_BLOCKS + UNIQUE_BLOCKS_PER_REQUEST;

    MultiWorkerResult result{};
    result.routing_name = routing_name;
    result.total_prefix_possible = TOTAL_REQUESTS * SHARED_PREFIX_BLOCKS;

    std::vector<NodePolicyFeatures> node_features;
    for (uint32_t i = 0; i < NUM_WORKERS; ++i) {
        node_features.push_back({NodeId{i}, 0, 0, 0, 0, 0, 0, 0, 0});
    }

    const CacheContextHash ctx_hash = computeContextHash(ctx);

    for (uint32_t req = 0; req < TOTAL_REQUESTS; ++req) {
        std::vector<TokenId> full = shared_prefix;
        const auto suffix = makeSuffix(req);
        full.insert(full.end(), suffix.begin(), suffix.end());

        const ContentHash first_block_hash = computeContentHash(
            ctx_hash, 0, static_cast<uint16_t>(BLOCK_SIZE),
            Span<const TokenId>{full.data(), BLOCK_SIZE});

        RoutingContext rctx{ctx_hash, total_blocks, first_block_hash, 0};
        const NodeId chosen_node = routing_policy.chooseNode(
            rctx, Span<const NodePolicyFeatures>{node_features.data(), node_features.size()});
        const uint32_t worker_idx = static_cast<uint32_t>(chosen_node);

        RouteRequest route_req{RequestId{req}, ctx, full};
        auto route_result = pipelines[worker_idx]->run(route_req);

        result.local_hits += route_result.local_hit_blocks;
        result.remote_hits += route_result.remote_hit_blocks;
        ++result.total_requests;

        // Admit missing computed blocks on the chosen worker.
        const uint32_t first_miss =
            static_cast<uint32_t>(route_result.reusable_blocks.size());
        for (uint32_t bi = first_miss; bi < total_blocks; ++bi) {
            auto cand = makeCandidate(ctx, bi, full);
            const ContentHash h = cand.hash;
            auto admit = rigs[worker_idx]->cache.admitBlock(cand);
            if (admit.status == AdmitResult::Status::Admitted && admit.handle.has_value()) {
                const BlockId bid = admit.handle->id();
                const uint64_t gen = admit.handle->metadata().generation;
                admit.handle.reset();
                coord_svc.announce(h, {chosen_node, bid, gen});
            }
        }
    }
    return result;
}

void printPhase2Result(const Phase2Result& r) {
    const double hit_rate =
        static_cast<double>(r.prefix_hits) / static_cast<double>(r.prefix_possible);
    std::cout << std::left << std::setw(20) << r.policy_name << "  "
              << "prefix_hit_rate=" << std::fixed << std::setprecision(3) << hit_rate << "  "
              << "evictions=" << r.evictions << "  admissions=" << r.admissions << "\n";
}

void printMultiWorkerResult(const MultiWorkerResult& r) {
    const uint32_t total_hits = r.local_hits + r.remote_hits;
    const double hit_rate =
        static_cast<double>(total_hits) / static_cast<double>(r.total_prefix_possible);
    const double remote_frac =
        total_hits > 0
            ? static_cast<double>(r.remote_hits) / static_cast<double>(total_hits)
            : 0.0;
    std::cout << std::left << std::setw(20) << r.routing_name << "  "
              << "prefix_hit_rate=" << std::fixed << std::setprecision(3) << hit_rate << "  "
              << "local=" << r.local_hits << "  remote=" << r.remote_hits << "  "
              << "remote_frac=" << std::setprecision(3) << remote_frac << "\n";
}

}  // namespace

int main() {
    std::cout << "kvcache_bench — Phase 4\n";
    std::cout << "requests=" << TOTAL_REQUESTS << "  workers=" << NUM_WORKERS
              << "  capacity_per_worker=" << WORKER_CAPACITY
              << "  block_size=" << BLOCK_SIZE << "  shared_prefix_blocks="
              << SHARED_PREFIX_BLOCKS << "\n\n";

    // Phase 2: single-node eviction policy comparison.
    std::cout << "── Phase 2: single-node eviction policy comparison ──────────────────\n";
    std::cout << std::left << std::setw(20) << "policy"
              << "  prefix_hit_rate  evictions  admissions\n";
    std::cout << std::string(64, '-') << "\n";

    LruEvictionPolicy lru;
    PrefixAwareEvictionPolicy prefix_aware;
    printPhase2Result(runSingleNodeBench("lru", lru));
    printPhase2Result(runSingleNodeBench("prefix_aware", prefix_aware));

    // Phase 4: multi-worker routing policy comparison.
    std::cout << "\n── Phase 4: multi-worker routing policy comparison ──────────────────\n";
    std::cout << std::left << std::setw(20) << "routing"
              << "  prefix_hit_rate  local  remote  remote_frac\n";
    std::cout << std::string(72, '-') << "\n";

    RandomRoutingPolicy random_routing{42};
    HashFirstBlockRoutingPolicy hash_routing;
    printMultiWorkerResult(runMultiWorkerBench("random", random_routing));
    printMultiWorkerResult(runMultiWorkerBench("hash_first_block", hash_routing));

    return 0;
}
