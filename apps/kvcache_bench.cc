#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "core/block_candidate.h"
#include "core/cache_key.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "core/ids.h"
#include "core/local_cache.h"
#include "core/span.h"
#include "policy/policies/lru_eviction.h"
#include "policy/policies/no_eviction.h"
#include "policy/policies/prefix_aware_eviction.h"
#include "policy/policies/always_admit.h"
#include "sim/decision_logger.h"
#include "sim/event_sink.h"

using namespace kvcache;
using namespace kvcache::policy;
using namespace kvcache::sim;

namespace {

constexpr uint32_t BLOCK_SIZE = 16;
constexpr uint32_t SHARED_PREFIX_BLOCKS = 4;
constexpr uint32_t SHARED_PREFIX_TOKENS = SHARED_PREFIX_BLOCKS * BLOCK_SIZE;
constexpr uint32_t UNIQUE_BLOCKS_PER_REQUEST = 1;
constexpr uint32_t TOTAL_REQUESTS = 200;
constexpr uint32_t CACHE_CAPACITY = SHARED_PREFIX_BLOCKS + 2;

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
        v[i] = static_cast<TokenId>(10000 + req_id * BLOCK_SIZE + i);
    }
    return v;
}

BlockCandidate makeCandidate(const CacheKeyContext& ctx, uint32_t block_idx,
                              const std::vector<TokenId>& full_tokens) {
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    const ContentHash h = computeContentHash(
        ctx_hash, block_idx, static_cast<uint16_t>(BLOCK_SIZE),
        Span<const TokenId>{full_tokens.data() + block_idx * BLOCK_SIZE, BLOCK_SIZE});
    return BlockCandidate{ctx, block_idx, full_tokens, h, {}, BlockOrigin::LocallyComputed,
                          std::nullopt};
}

struct BenchResult {
    std::string policy_name;
    uint32_t prefix_hits;
    uint32_t prefix_possible;
    uint32_t evictions;
    uint32_t admissions;
    uint32_t capacity_exceeded;
};

BenchResult runBench(const std::string& policy_name, EvictionPolicy& ep) {
    FakeClock clock;
    AlwaysAdmitPolicy ap;
    InMemoryEventSink events;
    NullDecisionLogger decisions;

    LocalCache cache{CACHE_CAPACITY, BLOCK_SIZE, clock, ap, ep, events, decisions};

    const auto ctx = makeCtx();
    const auto shared_prefix = makeSharedPrefix();
    const uint32_t total_blocks_in_req = SHARED_PREFIX_BLOCKS + UNIQUE_BLOCKS_PER_REQUEST;

    BenchResult result{};
    result.policy_name = policy_name;

    for (uint32_t req = 0; req < TOTAL_REQUESTS; ++req) {
        clock.advanceNs(1000);

        std::vector<TokenId> full = shared_prefix;
        const auto suffix = makeSuffix(req);
        full.insert(full.end(), suffix.begin(), suffix.end());

        auto outcome = cache.lookupPrefix(ctx, Span<const TokenId>{full.data(), full.size()});
        const uint32_t hit_blocks = static_cast<uint32_t>(outcome.matched_blocks.size());
        result.prefix_hits += std::min(hit_blocks, SHARED_PREFIX_BLOCKS);
        result.prefix_possible += SHARED_PREFIX_BLOCKS;

        const uint32_t first_miss = static_cast<uint32_t>(outcome.matched_blocks.size());
        for (uint32_t bi = first_miss; bi < total_blocks_in_req; ++bi) {
            std::vector<TokenId> toks_so_far(full.begin(),
                                              full.begin() + static_cast<ptrdiff_t>(
                                                                  (bi + 1) * BLOCK_SIZE));
            const auto admit = cache.admitBlock(makeCandidate(ctx, bi, toks_so_far));
            if (admit.status == AdmitResult::Status::Admitted) {
                ++result.admissions;
            } else if (admit.status == AdmitResult::Status::CapacityExceeded) {
                ++result.capacity_exceeded;
            }
        }
    }

    result.evictions = static_cast<uint32_t>(events.eventsOfType<BlockEvicted>().size());
    return result;
}

void printResult(const BenchResult& r) {
    const double hit_rate =
        static_cast<double>(r.prefix_hits) / static_cast<double>(r.prefix_possible);
    std::cout << std::left << std::setw(20) << r.policy_name << "  "
              << "prefix_hit_rate=" << std::fixed << std::setprecision(3) << hit_rate << "  "
              << "evictions=" << r.evictions << "  "
              << "admissions=" << r.admissions << "  "
              << "capacity_exceeded=" << r.capacity_exceeded << "\n";
}

}  // namespace

int main() {
    std::cout << "kvcache_bench — Phase 2\n";
    std::cout << "workload: shared_system_prompt  requests=" << TOTAL_REQUESTS
              << "  capacity=" << CACHE_CAPACITY << "  block_size=" << BLOCK_SIZE
              << "  shared_prefix_blocks=" << SHARED_PREFIX_BLOCKS << "\n\n";

    LruEvictionPolicy lru;
    NoEvictionPolicy no_evict;
    PrefixAwareEvictionPolicy prefix_aware;

    const auto lru_res = runBench("lru", lru);
    const auto no_res = runBench("no_eviction", no_evict);
    const auto pa_res = runBench("prefix_aware", prefix_aware);

    std::cout << std::left << std::setw(20) << "policy" << "  "
              << "prefix_hit_rate  evictions  admissions  capacity_exceeded\n";
    std::cout << std::string(72, '-') << "\n";
    printResult(lru_res);
    printResult(no_res);
    printResult(pa_res);

    return 0;
}
