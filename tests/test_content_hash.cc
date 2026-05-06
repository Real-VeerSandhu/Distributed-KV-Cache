#include <gtest/gtest.h>

#include <vector>

#include "core/cache_key.h"
#include "core/content_hash.h"
#include "core/ids.h"
#include "core/span.h"

namespace kvcache {

namespace {

CacheKeyContext makeTestContext() {
    return CacheKeyContext{
        .model_id = ModelId{1},
        .tokenizer_id = TokenizerId{1},
        .block_size = 16,
        .dtype = DType::BF16,
        .num_layers = 32,
        .num_kv_heads = 8,
        .head_dim = 128,
        .rope_config_hash = 0,
        .kv_layout_hash = 0,
    };
}

std::vector<TokenId> makeTokens(int start, int count) {
    std::vector<TokenId> tokens;
    tokens.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        tokens.push_back(start + i);
    }
    return tokens;
}

}  // namespace

TEST(ComputeContextHash, Deterministic) {
    const auto ctx = makeTestContext();
    EXPECT_EQ(computeContextHash(ctx), computeContextHash(ctx));
}

TEST(ComputeContextHash, DiffersOnModelId) {
    auto ctx_a = makeTestContext();
    auto ctx_b = makeTestContext();
    ctx_b.model_id = ModelId{2};
    EXPECT_NE(computeContextHash(ctx_a), computeContextHash(ctx_b));
}

TEST(ComputeContextHash, DiffersOnTokenizerId) {
    auto ctx_a = makeTestContext();
    auto ctx_b = makeTestContext();
    ctx_b.tokenizer_id = TokenizerId{99};
    EXPECT_NE(computeContextHash(ctx_a), computeContextHash(ctx_b));
}

TEST(ComputeContextHash, DiffersOnBlockSize) {
    auto ctx_a = makeTestContext();
    auto ctx_b = makeTestContext();
    ctx_b.block_size = 32;
    EXPECT_NE(computeContextHash(ctx_a), computeContextHash(ctx_b));
}

TEST(ComputeContextHash, DiffersOnDtype) {
    auto ctx_a = makeTestContext();
    auto ctx_b = makeTestContext();
    ctx_b.dtype = DType::FP16;
    EXPECT_NE(computeContextHash(ctx_a), computeContextHash(ctx_b));
}

TEST(ComputeContextHash, DiffersOnNumLayers) {
    auto ctx_a = makeTestContext();
    auto ctx_b = makeTestContext();
    ctx_b.num_layers = 16;
    EXPECT_NE(computeContextHash(ctx_a), computeContextHash(ctx_b));
}

TEST(ComputeContextHash, DiffersOnNumKvHeads) {
    auto ctx_a = makeTestContext();
    auto ctx_b = makeTestContext();
    ctx_b.num_kv_heads = 4;
    EXPECT_NE(computeContextHash(ctx_a), computeContextHash(ctx_b));
}

TEST(ComputeContextHash, DiffersOnHeadDim) {
    auto ctx_a = makeTestContext();
    auto ctx_b = makeTestContext();
    ctx_b.head_dim = 64;
    EXPECT_NE(computeContextHash(ctx_a), computeContextHash(ctx_b));
}

TEST(ComputeContextHash, DiffersOnRopeConfigHash) {
    auto ctx_a = makeTestContext();
    auto ctx_b = makeTestContext();
    ctx_b.rope_config_hash = 12345;
    EXPECT_NE(computeContextHash(ctx_a), computeContextHash(ctx_b));
}

TEST(ComputeContextHash, DiffersOnKvLayoutHash) {
    auto ctx_a = makeTestContext();
    auto ctx_b = makeTestContext();
    ctx_b.kv_layout_hash = 99999;
    EXPECT_NE(computeContextHash(ctx_a), computeContextHash(ctx_b));
}

TEST(ComputeContentHash, Deterministic) {
    const auto tokens = makeTokens(0, 4);
    const CacheContextHash ctx_hash = 42;
    const auto h1 = computeContentHash(ctx_hash, 0, 4, make_span(tokens));
    const auto h2 = computeContentHash(ctx_hash, 0, 4, make_span(tokens));
    EXPECT_EQ(h1, h2);
}

TEST(ComputeContentHash, DiffersOnBlockIndex) {
    const auto tokens = makeTokens(0, 4);
    const CacheContextHash ctx_hash = 42;
    const auto h1 = computeContentHash(ctx_hash, 0, 4, make_span(tokens));
    const auto h2 = computeContentHash(ctx_hash, 1, 4, make_span(tokens));
    EXPECT_NE(h1, h2);
}

TEST(ComputeContentHash, DiffersOnTokenContent) {
    const auto tokens_a = makeTokens(0, 4);
    const auto tokens_b = makeTokens(100, 4);
    const CacheContextHash ctx_hash = 42;
    const auto h1 = computeContentHash(ctx_hash, 0, 4, make_span(tokens_a));
    const auto h2 = computeContentHash(ctx_hash, 0, 4, make_span(tokens_b));
    EXPECT_NE(h1, h2);
}

TEST(ComputeContentHash, DiffersOnContextHash) {
    const auto tokens = makeTokens(0, 4);
    const auto h1 = computeContentHash(CacheContextHash{1}, 0, 4, make_span(tokens));
    const auto h2 = computeContentHash(CacheContextHash{2}, 0, 4, make_span(tokens));
    EXPECT_NE(h1, h2);
}

TEST(ComputeContentHash, DiffersOnTokenCount) {
    const auto tokens = makeTokens(0, 4);
    const CacheContextHash ctx_hash = 42;
    const auto h1 = computeContentHash(ctx_hash, 0, 3, make_span(tokens).subspan(0, 3));
    const auto h2 = computeContentHash(ctx_hash, 0, 4, make_span(tokens));
    EXPECT_NE(h1, h2);
}

TEST(ComputeContentHash, ProducesNonZeroHash) {
    const auto tokens = makeTokens(1, 4);
    const auto h = computeContentHash(CacheContextHash{1}, 0, 4, make_span(tokens));
    EXPECT_TRUE(h.lo != 0 || h.hi != 0);
}

TEST(ComputeContentHash, ContextHashBindsToResult) {
    const auto ctx = makeTestContext();
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    const auto tokens = makeTokens(0, 16);

    const auto h1 = computeContentHash(ctx_hash, 0, 16, make_span(tokens));
    const auto h2 = computeContentHash(ctx_hash + 1, 0, 16, make_span(tokens));
    EXPECT_NE(h1, h2);
}

}  // namespace kvcache
