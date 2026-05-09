#include "tier/tier_store.h"

#include <gtest/gtest.h>

using namespace kvcache;
using namespace kvcache::tier;

TEST(TierStore, InitiallyEmpty) {
    TierStore store{4};
    const auto stats = store.stats();
    EXPECT_EQ(stats.used_blocks, 0u);
    EXPECT_EQ(stats.capacity_blocks, 4u);
}

TEST(TierStore, HasCapacityWhenEmpty) {
    TierStore store{4};
    EXPECT_TRUE(store.hasCapacity());
}

TEST(TierStore, HasCapacityFalseWhenFull) {
    TierStore store{2};
    store.add(BlockId{0});
    store.add(BlockId{1});
    EXPECT_FALSE(store.hasCapacity());
}

TEST(TierStore, ContainsFalseForUntracked) {
    TierStore store{4};
    EXPECT_FALSE(store.contains(BlockId{5}));
}

TEST(TierStore, ContainsTrueAfterAdd) {
    TierStore store{4};
    store.add(BlockId{3});
    EXPECT_TRUE(store.contains(BlockId{3}));
}

TEST(TierStore, RemoveDecreasesUsedBlocks) {
    TierStore store{4};
    store.add(BlockId{1});
    store.remove(BlockId{1});
    EXPECT_EQ(store.stats().used_blocks, 0u);
    EXPECT_FALSE(store.contains(BlockId{1}));
}

TEST(TierStore, AddBeyondCapacityThrows) {
    TierStore store{1};
    store.add(BlockId{0});
    EXPECT_THROW(store.add(BlockId{1}), std::logic_error);
}

TEST(TierStore, StatsReflectsUsage) {
    TierStore store{8};
    store.add(BlockId{0});
    store.add(BlockId{1});
    store.add(BlockId{2});
    const auto stats = store.stats();
    EXPECT_EQ(stats.used_blocks, 3u);
    EXPECT_EQ(stats.capacity_blocks, 8u);
}

TEST(TierStore, BlockIdsContainsAddedBlocks) {
    TierStore store{8};
    store.add(BlockId{10});
    store.add(BlockId{20});
    const auto& ids = store.blockIds();
    EXPECT_EQ(ids.count(BlockId{10}), 1u);
    EXPECT_EQ(ids.count(BlockId{20}), 1u);
    EXPECT_EQ(ids.count(BlockId{30}), 0u);
}

TEST(TierStore, RemoveNonExistentBlockIsNoOp) {
    TierStore store{4};
    EXPECT_NO_THROW(store.remove(BlockId{99}));
}
