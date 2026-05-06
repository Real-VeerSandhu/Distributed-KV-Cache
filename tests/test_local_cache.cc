#include "core/local_cache.h"

#include <gtest/gtest.h>

#include "core/clock.h"
#include "core/content_hash.h"

using namespace kvcache;

namespace {

constexpr uint32_t BS = 4;

CacheKeyContext makeCtx(uint32_t block_size = BS) {
    return CacheKeyContext{ModelId{1},   TokenizerId{1}, block_size, DType::BF16,
                           32,           8,              128,        0,
                           0};
}

// Builds a BlockCandidate for block at block_index with full prefix tokens [0..end)
BlockCandidate makeCandidate(const CacheKeyContext& ctx, uint32_t block_idx,
                              std::initializer_list<int> all_tokens_so_far) {
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    std::vector<TokenId> toks;
    toks.reserve(all_tokens_so_far.size());
    for (int t : all_tokens_so_far) toks.push_back(static_cast<TokenId>(t));

    const ContentHash h = computeContentHash(
        ctx_hash, block_idx, static_cast<uint16_t>(ctx.block_size),
        Span<const TokenId>{toks.data() + block_idx * ctx.block_size, ctx.block_size});

    return BlockCandidate{ctx, block_idx, std::move(toks), h, {}, BlockOrigin::LocallyComputed,
                          std::nullopt};
}

}  // namespace

TEST(LocalCache, MissThenHitAfterAdmit) {
    FakeClock clock;
    LocalCache cache{16, BS, clock};
    const auto ctx = makeCtx();

    std::vector<TokenId> q = {1, 2, 3, 4};
    auto miss = cache.lookupPrefix(ctx, Span<const TokenId>{q.data(), q.size()});
    EXPECT_EQ(miss.matched_tokens, 0u);
    EXPECT_TRUE(miss.matched_blocks.empty());

    auto candidate = makeCandidate(ctx, 0, {1, 2, 3, 4});
    auto admit = cache.admitBlock(candidate);
    EXPECT_EQ(admit.status, AdmitResult::Status::Admitted);
    ASSERT_TRUE(admit.handle.has_value());

    auto hit = cache.lookupPrefix(ctx, Span<const TokenId>{q.data(), q.size()});
    EXPECT_EQ(hit.matched_tokens, BS);
    ASSERT_EQ(hit.matched_blocks.size(), 1u);
}

TEST(LocalCache, PartialTailNotAdmitted) {
    FakeClock clock;
    LocalCache cache{16, BS, clock};
    const auto ctx = makeCtx();

    // tokens.size() != (block_index+1)*block_size → InvalidBlock
    BlockCandidate bad{ctx, 0, {1, 2}, {0, 0}, {}, BlockOrigin::LocallyComputed, std::nullopt};
    auto result = cache.admitBlock(bad);
    EXPECT_EQ(result.status, AdmitResult::Status::InvalidBlock);
}

TEST(LocalCache, ContiguousPrefixOnlyReturned) {
    FakeClock clock;
    LocalCache cache{16, BS, clock};
    const auto ctx = makeCtx();

    // Admit block 0 and block 1 in order
    auto c0 = makeCandidate(ctx, 0, {1, 2, 3, 4});
    auto c1 = makeCandidate(ctx, 1, {1, 2, 3, 4, 5, 6, 7, 8});
    ASSERT_EQ(cache.admitBlock(c0).status, AdmitResult::Status::Admitted);
    ASSERT_EQ(cache.admitBlock(c1).status, AdmitResult::Status::Admitted);

    std::vector<TokenId> q = {1, 2, 3, 4, 5, 6, 7, 8};
    auto result = cache.lookupPrefix(ctx, Span<const TokenId>{q.data(), q.size()});
    EXPECT_EQ(result.matched_tokens, 2 * BS);
    EXPECT_EQ(result.matched_blocks.size(), 2u);
}

TEST(LocalCache, AdmitBlockOutOfOrderFails) {
    FakeClock clock;
    LocalCache cache{16, BS, clock};
    const auto ctx = makeCtx();

    // Block 1 before block 0 — prior chain incomplete
    auto c1 = makeCandidate(ctx, 1, {1, 2, 3, 4, 5, 6, 7, 8});
    auto result = cache.admitBlock(c1);
    EXPECT_EQ(result.status, AdmitResult::Status::InvalidBlock);
}

TEST(LocalCache, AlreadyPresentDetected) {
    FakeClock clock;
    LocalCache cache{16, BS, clock};
    const auto ctx = makeCtx();

    auto c = makeCandidate(ctx, 0, {1, 2, 3, 4});
    ASSERT_EQ(cache.admitBlock(c).status, AdmitResult::Status::Admitted);
    EXPECT_EQ(cache.admitBlock(c).status, AdmitResult::Status::AlreadyPresent);
}

TEST(LocalCache, CapacityExceededWhenFull) {
    FakeClock clock;
    LocalCache cache{1, BS, clock};  // capacity = 1 block
    const auto ctx = makeCtx();

    auto c0 = makeCandidate(ctx, 0, {1, 2, 3, 4});
    ASSERT_EQ(cache.admitBlock(c0).status, AdmitResult::Status::Admitted);

    // Use different tokens so it's a new block, not AlreadyPresent
    auto c1 = makeCandidate(ctx, 0, {5, 6, 7, 8});
    auto result = cache.admitBlock(c1);
    EXPECT_EQ(result.status, AdmitResult::Status::CapacityExceeded);
}

TEST(LocalCache, GetBlockHitAndMiss) {
    FakeClock clock;
    LocalCache cache{16, BS, clock};
    const auto ctx = makeCtx();

    auto c = makeCandidate(ctx, 0, {1, 2, 3, 4});
    auto admitted = cache.admitBlock(c);
    ASSERT_EQ(admitted.status, AdmitResult::Status::Admitted);
    const BlockId id = admitted.handle->id();
    const uint64_t gen = admitted.handle->metadata().generation;

    auto hit = cache.getBlock(id, gen);
    EXPECT_EQ(hit.status, FetchLocalResult::Status::Hit);

    auto miss = cache.getBlock(id, gen + 1);
    EXPECT_EQ(miss.status, FetchLocalResult::Status::StaleGeneration);
}

TEST(LocalCache, SnapshotReflectsAdmittedBlocks) {
    FakeClock clock;
    LocalCache cache{8, BS, clock};
    const auto ctx = makeCtx();

    auto snap0 = cache.snapshot();
    EXPECT_EQ(snap0.ready_blocks, 0u);
    EXPECT_EQ(snap0.total_capacity_blocks, 8u);

    auto c = makeCandidate(ctx, 0, {1, 2, 3, 4});
    ASSERT_EQ(cache.admitBlock(c).status, AdmitResult::Status::Admitted);

    auto snap1 = cache.snapshot();
    EXPECT_EQ(snap1.ready_blocks, 1u);
    EXPECT_EQ(snap1.used_blocks, 1u);
}

TEST(LocalCache, LookupContextHashIsCorrect) {
    FakeClock clock;
    LocalCache cache{16, BS, clock};
    const auto ctx = makeCtx();

    auto c = makeCandidate(ctx, 0, {1, 2, 3, 4});
    ASSERT_EQ(cache.admitBlock(c).status, AdmitResult::Status::Admitted);

    std::vector<TokenId> q = {1, 2, 3, 4};
    auto result = cache.lookupPrefix(ctx, Span<const TokenId>{q.data(), q.size()});
    EXPECT_EQ(result.context_hash, computeContextHash(ctx));
}
