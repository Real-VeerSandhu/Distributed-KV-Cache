#include <gtest/gtest.h>

#include <numeric>

#include "core/block_handle.h"
#include "core/cache_key.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "core/ids.h"
#include "core/local_cache.h"
#include "pipeline/missing_block_planner.h"
#include "pipeline/route_types.h"

using namespace kvcache;
using namespace kvcache::pipeline;

namespace {

constexpr uint32_t BS = 4;

CacheKeyContext makeCtx() {
    return CacheKeyContext{ModelId{1}, TokenizerId{1}, BS, DType::BF16, 32, 8, 128, 0, 0};
}

RouteRequest makeReq(std::initializer_list<TokenId> toks) {
    return {RequestId{1}, makeCtx(), std::vector<TokenId>(toks)};
}

LocalLookupOutcome emptyMatch() { return {}; }

LocalLookupOutcome matchedBlocks(uint32_t n, FakeClock& clock, LocalCache& cache,
                                  const CacheKeyContext& ctx,
                                  const std::vector<TokenId>& all_toks) {
    return cache.lookupPrefix(ctx, Span<const TokenId>{all_toks.data(), n * BS});
}

}  // namespace

TEST(MissingBlockPlanner, FullMissPlansAllBlocks) {
    MissingBlockPlanner planner;
    auto req = makeReq({1, 2, 3, 4, 5, 6, 7, 8});  // 2 full blocks
    LocalLookupOutcome local;

    auto missing = planner.plan(req, local);
    ASSERT_EQ(missing.size(), 2u);
    EXPECT_EQ(missing[0].block_index, 0u);
    EXPECT_EQ(missing[1].block_index, 1u);
}

TEST(MissingBlockPlanner, PartialMatchPlansRemainingBlocks) {
    MissingBlockPlanner planner;
    // Simulate 1 block matched locally by putting a fake handle-count in matched_blocks.
    // We test the plan() function directly: first_missing = local.matched_blocks.size().
    LocalLookupOutcome local;
    local.matched_tokens = BS;

    // Add a dummy handle count by constructing matched_blocks with size 1.
    // We can't easily create real BlockHandles here, but we can construct a rig.
    FakeClock clock;
    LocalCache cache(8, BS, clock);
    const auto ctx = makeCtx();
    std::vector<TokenId> toks = {1, 2, 3, 4, 5, 6, 7, 8};

    // Admit block 0.
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    const ContentHash h0 = computeContentHash(ctx_hash, 0, static_cast<uint16_t>(BS),
                                               Span<const TokenId>{toks.data(), BS});
    BlockCandidate cand0{ctx, 0, {1, 2, 3, 4}, h0, {}, BlockOrigin::LocallyComputed,
                          std::nullopt};
    cache.admitBlock(cand0);

    auto lookup_local = cache.lookupPrefix(ctx, Span<const TokenId>{toks.data(), toks.size()});
    ASSERT_EQ(lookup_local.matched_blocks.size(), 1u);

    RouteRequest req{RequestId{1}, ctx, toks};
    MissingBlockPlanner p;
    auto missing = p.plan(req, lookup_local);
    ASSERT_EQ(missing.size(), 1u);
    EXPECT_EQ(missing[0].block_index, 1u);
}

TEST(MissingBlockPlanner, NoMissingBlocksWhenAllMatched) {
    FakeClock clock;
    LocalCache cache(8, BS, clock);
    const auto ctx = makeCtx();
    std::vector<TokenId> toks = {1, 2, 3, 4};

    const CacheContextHash ctx_hash = computeContextHash(ctx);
    const ContentHash h0 = computeContentHash(ctx_hash, 0, static_cast<uint16_t>(BS),
                                               Span<const TokenId>{toks.data(), BS});
    BlockCandidate cand0{ctx, 0, toks, h0, {}, BlockOrigin::LocallyComputed, std::nullopt};
    cache.admitBlock(cand0);

    auto local = cache.lookupPrefix(ctx, Span<const TokenId>{toks.data(), toks.size()});
    ASSERT_EQ(local.matched_blocks.size(), 1u);

    RouteRequest req{RequestId{1}, ctx, toks};
    MissingBlockPlanner p;
    auto missing = p.plan(req, local);
    EXPECT_TRUE(missing.empty());
}

TEST(MissingBlockPlanner, HashesAreDeterministic) {
    MissingBlockPlanner planner;
    auto req = makeReq({1, 2, 3, 4, 5, 6, 7, 8});
    LocalLookupOutcome local;

    auto m1 = planner.plan(req, local);
    auto m2 = planner.plan(req, local);

    ASSERT_EQ(m1.size(), m2.size());
    for (std::size_t i = 0; i < m1.size(); ++i) {
        EXPECT_EQ(m1[i].hash, m2[i].hash);
        EXPECT_EQ(m1[i].block_index, m2[i].block_index);
    }
}

TEST(MissingBlockPlanner, DifferentTokensProduceDifferentHashes) {
    MissingBlockPlanner planner;
    auto req1 = makeReq({1, 2, 3, 4});
    auto req2 = makeReq({5, 6, 7, 8});
    LocalLookupOutcome local;

    auto m1 = planner.plan(req1, local);
    auto m2 = planner.plan(req2, local);

    ASSERT_EQ(m1.size(), 1u);
    ASSERT_EQ(m2.size(), 1u);
    EXPECT_NE(m1[0].hash, m2[0].hash);
}

TEST(MissingBlockPlanner, PartialTailTokensIgnored) {
    MissingBlockPlanner planner;
    // 5 tokens with BS=4 → 1 full block, 1 partial token (ignored)
    auto req = makeReq({1, 2, 3, 4, 5});
    LocalLookupOutcome local;
    auto missing = planner.plan(req, local);
    ASSERT_EQ(missing.size(), 1u);
    EXPECT_EQ(missing[0].block_index, 0u);
}
