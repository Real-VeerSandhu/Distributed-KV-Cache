#include "prefix/prefix_lookup_engine.h"

#include <gtest/gtest.h>

#include "core/block_candidate.h"
#include "core/clock.h"
#include "core/block_manager.h"
#include "prefix/prefix_index.h"

using namespace kvcache;
using namespace kvcache::prefix;

namespace {

constexpr uint32_t BS = 4;
constexpr CacheContextHash CTX = 0x1111;

Span<const TokenId> span(const std::vector<TokenId>& v) {
    return Span<const TokenId>(v.data(), v.size());
}

std::vector<TokenId> toks(std::initializer_list<int> vals) {
    std::vector<TokenId> v;
    for (int x : vals) v.push_back(static_cast<TokenId>(x));
    return v;
}

struct Fixture {
    FakeClock clock;
    BlockManager mgr{8, clock};
    PrefixIndex idx{BS};
    PrefixLookupEngine engine{idx, mgr};

    // Admits a block and inserts it into the prefix index. Returns the BlockId.
    BlockId admit(uint32_t block_idx, const std::vector<TokenId>& full_tokens,
                  ContentHash hash = {1, 2}) {
        auto slot = mgr.createBlock(hash, block_idx, static_cast<uint16_t>(BS), Tier::GpuSim);
        EXPECT_TRUE(slot.has_value());
        const auto [id, gen] = *slot;
        (void)gen;

        // Build block list (prior blocks come from index lookup)
        auto existing = idx.lookup(CTX, Span<const TokenId>{full_tokens.data(),
                                                              block_idx * BS});
        std::vector<BlockId> all = existing.blocks;
        all.push_back(id);

        idx.insert(CTX, Span<const TokenId>{full_tokens.data(), full_tokens.size()},
                   Span<const BlockId>{all.data(), all.size()});

        mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);
        return id;
    }
};

}  // namespace

TEST(PrefixLookupEngine, EmptyIndexReturnsNothing) {
    Fixture f;
    auto q = toks({1, 2, 3, 4});
    auto result = f.engine.lookup(CTX, span(q), BS);
    EXPECT_TRUE(result.handles.empty());
    EXPECT_EQ(result.matched_tokens, 0u);
}

TEST(PrefixLookupEngine, SingleReadyBlockReturnsHandle) {
    Fixture f;
    auto t = toks({1, 2, 3, 4});
    f.admit(0, t);

    auto result = f.engine.lookup(CTX, span(t), BS);
    ASSERT_EQ(result.handles.size(), 1u);
    EXPECT_EQ(result.matched_tokens, BS);
    EXPECT_TRUE(result.handles[0].valid());
}

TEST(PrefixLookupEngine, NonReadyBlockIsSkipped) {
    Fixture f;
    auto t = toks({1, 2, 3, 4});

    // Create block but leave it in Admitting state
    auto slot = f.mgr.createBlock({1, 2}, 0, static_cast<uint16_t>(BS), Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    const auto [id, gen] = *slot;
    (void)gen;
    std::vector<BlockId> bids{id};
    f.idx.insert(CTX, span(t), Span<const BlockId>{bids.data(), bids.size()});
    // NOT transitioned to Ready

    auto result = f.engine.lookup(CTX, span(t), BS);
    EXPECT_TRUE(result.handles.empty());
    EXPECT_EQ(result.matched_tokens, 0u);
}

TEST(PrefixLookupEngine, StopsAtFirstMissingBlock) {
    Fixture f;
    auto t2 = toks({1, 2, 3, 4, 5, 6, 7, 8});

    // Admit both blocks
    f.admit(0, {1, 2, 3, 4});
    auto t1 = toks({1, 2, 3, 4});
    BlockId id1 = f.admit(1, t2);

    // Evict block 1 from the index (remove it so lookup stops)
    f.idx.remove(id1);

    auto result = f.engine.lookup(CTX, span(t2), BS);
    ASSERT_EQ(result.handles.size(), 1u);
    EXPECT_EQ(result.matched_tokens, BS);
}

TEST(PrefixLookupEngine, FullPrefixTwoBlocks) {
    Fixture f;
    auto t = toks({1, 2, 3, 4, 5, 6, 7, 8});
    f.admit(0, {1, 2, 3, 4});
    f.admit(1, t);

    auto result = f.engine.lookup(CTX, span(t), BS);
    ASSERT_EQ(result.handles.size(), 2u);
    EXPECT_EQ(result.matched_tokens, 2 * BS);
}

TEST(PrefixLookupEngine, ContextMismatchReturnsNothing) {
    Fixture f;
    auto t = toks({1, 2, 3, 4});
    f.admit(0, t);

    constexpr CacheContextHash OTHER_CTX = 0x9999;
    auto result = f.engine.lookup(OTHER_CTX, span(t), BS);
    EXPECT_TRUE(result.handles.empty());
}

TEST(PrefixLookupEngine, PartialTokensReturnNoBlocks) {
    Fixture f;
    auto t = toks({1, 2, 3, 4});
    f.admit(0, t);

    // Query with fewer tokens than block_size — not enough to complete the edge
    auto q = toks({1, 2});
    auto result = f.engine.lookup(CTX, span(q), BS);
    EXPECT_TRUE(result.handles.empty());
}
