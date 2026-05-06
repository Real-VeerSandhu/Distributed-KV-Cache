#include "prefix/prefix_index.h"

#include <gtest/gtest.h>

using namespace kvcache;
using namespace kvcache::prefix;

namespace {

constexpr uint32_t BS = 4;  // block_size for all tests
constexpr CacheContextHash CTX_A = 0xAAAAAAAA;
constexpr CacheContextHash CTX_B = 0xBBBBBBBB;

std::vector<TokenId> toks(std::initializer_list<int> vals) {
    std::vector<TokenId> v;
    v.reserve(vals.size());
    for (int x : vals) v.push_back(static_cast<TokenId>(x));
    return v;
}

Span<const TokenId> span(const std::vector<TokenId>& v) {
    return Span<const TokenId>(v.data(), v.size());
}

Span<const BlockId> span(const std::vector<BlockId>& v) {
    return Span<const BlockId>(v.data(), v.size());
}

}  // namespace

TEST(PrefixIndex, EmptyLookupReturnsNothing) {
    PrefixIndex idx{BS};
    auto t = toks({1, 2, 3, 4});
    auto result = idx.lookup(CTX_A, span(t));
    EXPECT_TRUE(result.blocks.empty());
    EXPECT_EQ(result.matched_tokens, 0u);
}

TEST(PrefixIndex, LookupUnknownContextReturnsNothing) {
    PrefixIndex idx{BS};
    auto t = toks({1, 2, 3, 4});
    auto bids = std::vector<BlockId>{BlockId{1}};
    idx.insert(CTX_A, span(t), span(bids));

    auto result = idx.lookup(CTX_B, span(t));
    EXPECT_TRUE(result.blocks.empty());
}

TEST(PrefixIndex, SingleInsertThenLookupHits) {
    PrefixIndex idx{BS};
    auto t = toks({1, 2, 3, 4});
    auto bids = std::vector<BlockId>{BlockId{42}};
    idx.insert(CTX_A, span(t), span(bids));

    auto result = idx.lookup(CTX_A, span(t));
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(result.blocks[0], BlockId{42});
    EXPECT_EQ(result.matched_tokens, BS);
}

TEST(PrefixIndex, LookupWithNoMatchingTokensReturnsNothing) {
    PrefixIndex idx{BS};
    auto t = toks({1, 2, 3, 4});
    auto bids = std::vector<BlockId>{BlockId{1}};
    idx.insert(CTX_A, span(t), span(bids));

    auto q = toks({9, 9, 9, 9});
    auto result = idx.lookup(CTX_A, span(q));
    EXPECT_TRUE(result.blocks.empty());
}

TEST(PrefixIndex, SharedPrefixInsertTwoBlocks) {
    PrefixIndex idx{BS};
    // Insert two-block sequence
    auto t = toks({1, 2, 3, 4, 5, 6, 7, 8});
    auto bids = std::vector<BlockId>{BlockId{1}, BlockId{2}};
    idx.insert(CTX_A, span(t), span(bids));

    auto result = idx.lookup(CTX_A, span(t));
    ASSERT_EQ(result.blocks.size(), 2u);
    EXPECT_EQ(result.blocks[0], BlockId{1});
    EXPECT_EQ(result.blocks[1], BlockId{2});
    EXPECT_EQ(result.matched_tokens, 2 * BS);
}

TEST(PrefixIndex, LookupPartialMatchReturnsFirstBlockOnly) {
    PrefixIndex idx{BS};
    auto t = toks({1, 2, 3, 4, 5, 6, 7, 8});
    auto bids = std::vector<BlockId>{BlockId{1}, BlockId{2}};
    idx.insert(CTX_A, span(t), span(bids));

    // Query only one block worth
    auto q = toks({1, 2, 3, 4});
    auto result = idx.lookup(CTX_A, span(q));
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(result.blocks[0], BlockId{1});
    EXPECT_EQ(result.matched_tokens, BS);
}

TEST(PrefixIndex, EdgeSplitOnDivergingFirstBlock) {
    PrefixIndex idx{BS};

    // Insert sequence A: [1,2,3,4]
    auto t1 = toks({1, 2, 3, 4});
    auto b1 = std::vector<BlockId>{BlockId{10}};
    idx.insert(CTX_A, span(t1), span(b1));

    // Insert sequence B: shares prefix [1,2] but diverges at token 3
    auto t2 = toks({1, 2, 5, 6});
    auto b2 = std::vector<BlockId>{BlockId{20}};
    idx.insert(CTX_A, span(t2), span(b2));

    auto r1 = idx.lookup(CTX_A, span(t1));
    ASSERT_EQ(r1.blocks.size(), 1u);
    EXPECT_EQ(r1.blocks[0], BlockId{10});

    auto r2 = idx.lookup(CTX_A, span(t2));
    ASSERT_EQ(r2.blocks.size(), 1u);
    EXPECT_EQ(r2.blocks[0], BlockId{20});
}

TEST(PrefixIndex, SharedFirstBlockDifferentSecondBlock) {
    PrefixIndex idx{BS};

    // Both sequences share block 0 (same tokens), differ in block 1
    auto t1 = toks({1, 2, 3, 4, 5, 6, 7, 8});
    auto b1 = std::vector<BlockId>{BlockId{1}, BlockId{2}};
    idx.insert(CTX_A, span(t1), span(b1));

    auto t2 = toks({1, 2, 3, 4, 9, 10, 11, 12});
    auto b2 = std::vector<BlockId>{BlockId{1}, BlockId{3}};
    idx.insert(CTX_A, span(t2), span(b2));

    auto r1 = idx.lookup(CTX_A, span(t1));
    ASSERT_EQ(r1.blocks.size(), 2u);
    EXPECT_EQ(r1.blocks[0], BlockId{1});
    EXPECT_EQ(r1.blocks[1], BlockId{2});

    auto r2 = idx.lookup(CTX_A, span(t2));
    ASSERT_EQ(r2.blocks.size(), 2u);
    EXPECT_EQ(r2.blocks[0], BlockId{1});
    EXPECT_EQ(r2.blocks[1], BlockId{3});
}

TEST(PrefixIndex, RemoveLeafPrunesNode) {
    PrefixIndex idx{BS};
    auto t = toks({1, 2, 3, 4});
    auto bids = std::vector<BlockId>{BlockId{7}};
    idx.insert(CTX_A, span(t), span(bids));

    idx.remove(BlockId{7});

    auto result = idx.lookup(CTX_A, span(t));
    EXPECT_TRUE(result.blocks.empty());
}

TEST(PrefixIndex, RemoveLeafAndLookupShortPathStillWorks) {
    PrefixIndex idx{BS};

    auto t_long = toks({1, 2, 3, 4, 5, 6, 7, 8});
    auto b_long = std::vector<BlockId>{BlockId{1}, BlockId{2}};
    idx.insert(CTX_A, span(t_long), span(b_long));

    // Remove block 2 (leaf)
    idx.remove(BlockId{2});

    // Block 1 at depth 1 should still be found
    auto q = toks({1, 2, 3, 4});
    auto result = idx.lookup(CTX_A, span(q));
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(result.blocks[0], BlockId{1});
}

TEST(PrefixIndex, RemoveInternalBlockStopsLookup) {
    PrefixIndex idx{BS};

    auto t = toks({1, 2, 3, 4, 5, 6, 7, 8});
    auto bids = std::vector<BlockId>{BlockId{1}, BlockId{2}};
    idx.insert(CTX_A, span(t), span(bids));

    // Remove block 1 (internal node — has a child for block 2)
    idx.remove(BlockId{1});

    // Full lookup should stop at depth 4 (no terminal there now)
    auto result = idx.lookup(CTX_A, span(t));
    EXPECT_TRUE(result.blocks.empty());
    EXPECT_EQ(result.matched_tokens, 0u);
}

TEST(PrefixIndex, PruneEmptyNodesOnRemoveLeaf) {
    PrefixIndex idx{BS};
    // Single block — remove it; the tree should be empty (no crash on re-insert)
    auto t = toks({1, 2, 3, 4});
    auto bids = std::vector<BlockId>{BlockId{5}};
    idx.insert(CTX_A, span(t), span(bids));
    idx.remove(BlockId{5});

    // Re-insert same sequence with a new block
    auto bids2 = std::vector<BlockId>{BlockId{6}};
    idx.insert(CTX_A, span(t), span(bids2));
    auto result = idx.lookup(CTX_A, span(t));
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(result.blocks[0], BlockId{6});
}

TEST(PrefixIndex, ContiguousLookupStopsAtGap) {
    PrefixIndex idx{BS};

    auto t = toks({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
    auto bids = std::vector<BlockId>{BlockId{1}, BlockId{2}, BlockId{3}};
    idx.insert(CTX_A, span(t), span(bids));

    // Remove block 2 (middle) — lookup should return block 1 only
    idx.remove(BlockId{2});

    auto result = idx.lookup(CTX_A, span(t));
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(result.blocks[0], BlockId{1});
    EXPECT_EQ(result.matched_tokens, BS);
}

TEST(PrefixIndex, NamespaceSeparation) {
    PrefixIndex idx{BS};
    auto t = toks({1, 2, 3, 4});
    auto b_a = std::vector<BlockId>{BlockId{10}};
    auto b_b = std::vector<BlockId>{BlockId{20}};
    idx.insert(CTX_A, span(t), span(b_a));
    idx.insert(CTX_B, span(t), span(b_b));

    auto ra = idx.lookup(CTX_A, span(t));
    ASSERT_EQ(ra.blocks.size(), 1u);
    EXPECT_EQ(ra.blocks[0], BlockId{10});

    auto rb = idx.lookup(CTX_B, span(t));
    ASSERT_EQ(rb.blocks.size(), 1u);
    EXPECT_EQ(rb.blocks[0], BlockId{20});
}

TEST(PrefixIndex, RemoveNonExistentIdIsNoOp) {
    PrefixIndex idx{BS};
    EXPECT_NO_THROW(idx.remove(BlockId{999}));
}

TEST(PrefixIndex, DuplicateInsertDoesNotOverwriteTerminal) {
    PrefixIndex idx{BS};
    auto t = toks({1, 2, 3, 4});
    auto b1 = std::vector<BlockId>{BlockId{10}};
    auto b2 = std::vector<BlockId>{BlockId{20}};
    idx.insert(CTX_A, span(t), span(b1));
    idx.insert(CTX_A, span(t), span(b2));  // second insert, same position

    auto result = idx.lookup(CTX_A, span(t));
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(result.blocks[0], BlockId{10});  // first insertion wins
}
