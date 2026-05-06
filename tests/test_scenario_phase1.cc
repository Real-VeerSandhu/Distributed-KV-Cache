#include "core/local_cache.h"

#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "core/clock.h"
#include "core/content_hash.h"

using namespace kvcache;

// Shared-prefix workload: 100 requests each sharing a 64-token prefix.
// After the prefix, each request has a unique 16-token suffix.
// block_size = 16, so the shared prefix = 4 blocks.
// Expected: after the first request warms the cache, all subsequent requests
// find all 4 prefix blocks in cache → hit rate ≥ 80%.

namespace {

constexpr uint32_t BLOCK_SIZE = 16;
constexpr uint32_t SHARED_PREFIX_TOKENS = 64;
constexpr uint32_t SHARED_BLOCKS = SHARED_PREFIX_TOKENS / BLOCK_SIZE;  // 4
constexpr uint32_t CACHE_CAPACITY = 64;
constexpr uint32_t TOTAL_REQUESTS = 100;

CacheKeyContext makeCtx() {
    return CacheKeyContext{ModelId{1}, TokenizerId{1}, BLOCK_SIZE, DType::BF16,
                           32,        8,               128,         0,
                           0};
}

// Fixed shared prefix tokens
std::vector<TokenId> makeSharedPrefix() {
    std::vector<TokenId> v(SHARED_PREFIX_TOKENS);
    std::iota(v.begin(), v.end(), TokenId{1});
    return v;
}

// Unique suffix per request (16 tokens)
std::vector<TokenId> makeSuffix(uint32_t request_id) {
    std::vector<TokenId> v(BLOCK_SIZE);
    for (uint32_t i = 0; i < BLOCK_SIZE; ++i) {
        v[i] = static_cast<TokenId>(1000 + request_id * BLOCK_SIZE + i);
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

}  // namespace

TEST(ScenarioPhase1, SharedPrefixHitRateAbove80Percent) {
    FakeClock clock;
    LocalCache cache{CACHE_CAPACITY, BLOCK_SIZE, clock};
    const auto ctx = makeCtx();
    const auto shared_prefix = makeSharedPrefix();

    uint32_t total_possible_prefix_hits = 0;
    uint32_t actual_prefix_hits = 0;

    for (uint32_t req = 0; req < TOTAL_REQUESTS; ++req) {
        clock.advanceNs(1000);

        // Build full token sequence: shared prefix + unique suffix
        std::vector<TokenId> full_tokens = shared_prefix;
        const auto suffix = makeSuffix(req);
        full_tokens.insert(full_tokens.end(), suffix.begin(), suffix.end());

        // 1. Lookup prefix
        auto outcome = cache.lookupPrefix(
            ctx, Span<const TokenId>{full_tokens.data(), full_tokens.size()});

        const uint32_t hit_blocks =
            static_cast<uint32_t>(outcome.matched_blocks.size());
        actual_prefix_hits += hit_blocks;
        total_possible_prefix_hits += SHARED_BLOCKS;

        // 2. Admit missed prefix blocks (those not already in cache)
        const uint32_t first_miss_block =
            static_cast<uint32_t>(outcome.matched_blocks.size());
        for (uint32_t bi = first_miss_block; bi < SHARED_BLOCKS; ++bi) {
            std::vector<TokenId> tokens_so_far(
                full_tokens.begin(),
                full_tokens.begin() +
                    static_cast<ptrdiff_t>((bi + 1) * BLOCK_SIZE));
            auto candidate = makeCandidate(ctx, bi, tokens_so_far);
            auto admit = cache.admitBlock(candidate);
            EXPECT_TRUE(admit.status == AdmitResult::Status::Admitted ||
                        admit.status == AdmitResult::Status::AlreadyPresent)
                << "req=" << req << " block=" << bi;
        }
    }

    const double hit_rate =
        static_cast<double>(actual_prefix_hits) /
        static_cast<double>(total_possible_prefix_hits);

    // First request: 0/4 hits. Subsequent 99 requests: 4/4 hits each.
    // Expected hit rate: (99 * 4) / (100 * 4) = 396/400 = 0.99
    EXPECT_GE(hit_rate, 0.80)
        << "Hit rate " << hit_rate << " is below 80%";
}

TEST(ScenarioPhase1, CacheSaturatesGracefully) {
    FakeClock clock;
    // Very small cache: only 2 blocks
    LocalCache cache{2, BLOCK_SIZE, clock};
    const auto ctx = makeCtx();

    // Admit 2 blocks — fills capacity
    const auto prefix = makeSharedPrefix();
    for (uint32_t bi = 0; bi < 2; ++bi) {
        std::vector<TokenId> toks(prefix.begin(),
                                   prefix.begin() +
                                       static_cast<ptrdiff_t>((bi + 1) * BLOCK_SIZE));
        auto candidate = makeCandidate(ctx, bi, toks);
        auto result = cache.admitBlock(candidate);
        EXPECT_EQ(result.status, AdmitResult::Status::Admitted);
    }

    // Third block should fail with capacity exceeded
    std::vector<TokenId> toks3(prefix.begin(),
                                prefix.begin() +
                                    static_cast<ptrdiff_t>(3 * BLOCK_SIZE));
    auto candidate3 = makeCandidate(ctx, 2, toks3);
    auto result3 = cache.admitBlock(candidate3);
    EXPECT_EQ(result3.status, AdmitResult::Status::CapacityExceeded);
}
