#include "core/local_cache.h"

#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "core/clock.h"
#include "core/content_hash.h"
#include "policy/policies/always_admit.h"
#include "policy/policies/lru_eviction.h"
#include "policy/policies/no_eviction.h"
#include "policy/policies/prefix_aware_eviction.h"
#include "sim/decision_logger.h"
#include "sim/event_sink.h"

using namespace kvcache;
using namespace kvcache::policy;
using namespace kvcache::sim;

// Workload: shared_system_prompt style.
// block_size=4, shared prefix=3 blocks (12 tokens), 1 unique block per request.
// Cache capacity=3 (exactly the shared prefix size).

namespace {

constexpr uint32_t BLOCK_SIZE = 4;
constexpr uint32_t SHARED_BLOCKS = 3;
constexpr uint32_t SHARED_TOKENS = SHARED_BLOCKS * BLOCK_SIZE;
constexpr uint32_t UNIQUE_BLOCKS = 1;
constexpr uint32_t TOTAL_REQUESTS = 20;
// Small cache: just the shared prefix. No room for unique blocks unless evicted.
constexpr uint32_t CACHE_CAPACITY = SHARED_BLOCKS + UNIQUE_BLOCKS;  // 4

CacheKeyContext makeCtx() {
    return CacheKeyContext{ModelId{1}, TokenizerId{1}, BLOCK_SIZE, DType::BF16, 32, 8, 128, 0, 0};
}

std::vector<TokenId> sharedPrefix() {
    std::vector<TokenId> v(SHARED_TOKENS);
    std::iota(v.begin(), v.end(), TokenId{1});
    return v;
}

std::vector<TokenId> uniqueBlock(uint32_t req_id) {
    std::vector<TokenId> v(BLOCK_SIZE);
    for (uint32_t i = 0; i < BLOCK_SIZE; ++i) {
        v[i] = static_cast<TokenId>(1000 + req_id * BLOCK_SIZE + i);
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

struct WorkloadResult {
    uint32_t prefix_hits;
    uint32_t prefix_possible;
    uint32_t eviction_count;
};

WorkloadResult runWorkload(LocalCache& cache, InMemoryEventSink& events) {
    const auto ctx = makeCtx();
    const auto prefix = sharedPrefix();

    WorkloadResult res{};

    for (uint32_t req = 0; req < TOTAL_REQUESTS; ++req) {
        // Build full token sequence: shared prefix + unique suffix
        std::vector<TokenId> full = prefix;
        const auto suffix = uniqueBlock(req);
        full.insert(full.end(), suffix.begin(), suffix.end());

        auto outcome =
            cache.lookupPrefix(ctx, Span<const TokenId>{full.data(), full.size()});

        const uint32_t hit_blocks = static_cast<uint32_t>(outcome.matched_blocks.size());
        res.prefix_hits += std::min(hit_blocks, SHARED_BLOCKS);
        res.prefix_possible += SHARED_BLOCKS;

        // Admit missed prefix blocks
        const uint32_t first_miss = static_cast<uint32_t>(outcome.matched_blocks.size());
        for (uint32_t bi = first_miss; bi < SHARED_BLOCKS; ++bi) {
            std::vector<TokenId> toks_so_far(full.begin(),
                                              full.begin() + static_cast<ptrdiff_t>(
                                                                  (bi + 1) * BLOCK_SIZE));
            (void)cache.admitBlock(makeCandidate(ctx, bi, toks_so_far));
        }

        // Admit unique block (may fail if capacity exceeded — that's ok)
        (void)cache.admitBlock(makeCandidate(ctx, SHARED_BLOCKS, full));
    }

    res.eviction_count = static_cast<uint32_t>(events.eventsOfType<BlockEvicted>().size());
    return res;
}

}  // namespace

// With NoEviction and capacity=4: first request fills the cache (3 prefix + 1 unique).
// Subsequent requests: prefix hits=3, unique admissions fail (capacity full). Hit rate = ~99%.
TEST(ScenarioPhase2, NoEvictionProtectsPrefixBlocks) {
    FakeClock clock;
    NoEvictionPolicy ep;
    AlwaysAdmitPolicy ap;
    InMemoryEventSink events;
    InMemoryDecisionLogger decisions;

    LocalCache cache{CACHE_CAPACITY, BLOCK_SIZE, clock, ap, ep, events, decisions};

    const auto res = runWorkload(cache, events);

    const double hit_rate =
        static_cast<double>(res.prefix_hits) / static_cast<double>(res.prefix_possible);
    EXPECT_GE(hit_rate, 0.90) << "NoEviction hit rate: " << hit_rate;
    EXPECT_EQ(res.eviction_count, 0u);
}

// With LRU and capacity=4: prefix blocks stay warm (accessed every request), unique block
// gets evicted each time. Hit rate should remain high.
TEST(ScenarioPhase2, LruEvictionMaintainsHighPrefixHitRate) {
    FakeClock clock;
    LruEvictionPolicy ep;
    AlwaysAdmitPolicy ap;
    InMemoryEventSink events;
    InMemoryDecisionLogger decisions;

    LocalCache cache{CACHE_CAPACITY, BLOCK_SIZE, clock, ap, ep, events, decisions};

    const auto res = runWorkload(cache, events);

    const double hit_rate =
        static_cast<double>(res.prefix_hits) / static_cast<double>(res.prefix_possible);
    EXPECT_GE(hit_rate, 0.90) << "LRU hit rate: " << hit_rate;
    // LRU should have evicted unique blocks (one per request after warmup)
    EXPECT_GT(res.eviction_count, 0u);
}

// PrefixAware eviction should also protect prefix blocks.
TEST(ScenarioPhase2, PrefixAwareEvictionProtectsSharedPrefixBlocks) {
    FakeClock clock;
    PrefixAwareEvictionPolicy ep;
    AlwaysAdmitPolicy ap;
    InMemoryEventSink events;
    InMemoryDecisionLogger decisions;

    LocalCache cache{CACHE_CAPACITY, BLOCK_SIZE, clock, ap, ep, events, decisions};

    const auto res = runWorkload(cache, events);

    const double hit_rate =
        static_cast<double>(res.prefix_hits) / static_cast<double>(res.prefix_possible);
    EXPECT_GE(hit_rate, 0.90) << "PrefixAware hit rate: " << hit_rate;
}

// Pressure scenario: cache too small for prefix. LRU must sometimes evict prefix blocks.
// With PrefixAware, prefix blocks score higher and are protected longer.
TEST(ScenarioPhase2, PrefixAwareOutperformsLruUnderPrefixPressure) {
    // Cache smaller than the shared prefix: capacity = SHARED_BLOCKS - 1 = 2
    // Under pure LRU, any block including prefix roots may be evicted.
    // Under PrefixAware, the root blocks with high child counts are protected.
    constexpr uint32_t SMALL_CAPACITY = 2;

    auto runWithPolicy = [&](auto& ep) {
        FakeClock clock;
        AlwaysAdmitPolicy ap;
        InMemoryEventSink events;
        InMemoryDecisionLogger decisions;
        LocalCache cache{SMALL_CAPACITY, BLOCK_SIZE, clock, ap, ep, events, decisions};
        return runWorkload(cache, events);
    };

    LruEvictionPolicy lru;
    PrefixAwareEvictionPolicy pa;

    const auto lru_res = runWithPolicy(lru);
    const auto pa_res = runWithPolicy(pa);

    // Both should have evictions in this pressure scenario.
    EXPECT_GT(lru_res.eviction_count, 0u);
    EXPECT_GT(pa_res.eviction_count, 0u);

    // PrefixAware hit rate should be >= LRU (often better due to prefix protection).
    const double lru_rate = static_cast<double>(lru_res.prefix_hits) /
                            static_cast<double>(lru_res.prefix_possible);
    const double pa_rate =
        static_cast<double>(pa_res.prefix_hits) / static_cast<double>(pa_res.prefix_possible);

    EXPECT_GE(pa_rate, lru_rate)
        << "PrefixAware (" << pa_rate << ") should >= LRU (" << lru_rate << ")";
}

// Eviction decisions should be logged.
TEST(ScenarioPhase2, EvictionDecisionsAreLogged) {
    FakeClock clock;
    LruEvictionPolicy ep;
    AlwaysAdmitPolicy ap;
    InMemoryEventSink events;
    InMemoryDecisionLogger decisions;

    LocalCache cache{CACHE_CAPACITY, BLOCK_SIZE, clock, ap, ep, events, decisions};
    runWorkload(cache, events);

    const auto logged = decisions.decisionsOfType<EvictionDecision>();
    EXPECT_GT(logged.size(), 0u);
    for (const auto& d : logged) {
        EXPECT_EQ(d.policy_name, "lru");
        EXPECT_FALSE(d.candidates.empty());
    }
}

// Admission decisions should be logged.
TEST(ScenarioPhase2, AdmissionDecisionsAreLogged) {
    FakeClock clock;
    LruEvictionPolicy ep;
    AlwaysAdmitPolicy ap;
    InMemoryEventSink events;
    InMemoryDecisionLogger decisions;

    LocalCache cache{CACHE_CAPACITY, BLOCK_SIZE, clock, ap, ep, events, decisions};
    runWorkload(cache, events);

    const auto logged = decisions.decisionsOfType<sim::AdmissionRecord>();
    EXPECT_GT(logged.size(), 0u);
    for (const auto& d : logged) {
        EXPECT_EQ(d.policy_name, "always_admit");
        EXPECT_TRUE(d.admitted);
    }
}
