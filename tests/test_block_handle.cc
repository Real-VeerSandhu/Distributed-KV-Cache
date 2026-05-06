#include "core/block_handle.h"

#include <gtest/gtest.h>

#include "core/block_store.h"

using namespace kvcache;

namespace {
BlockStore makeStore(uint32_t capacity = 4) {
    return BlockStore{capacity};
}

void initSlot(BlockStore& store, BlockId id, BlockState state = BlockState::Ready) {
    store.initialize(id, 1u, {0, 0}, 0, 4, Tier::GpuSim, state);
}
}  // namespace

TEST(BlockHandle, ConstructIncreasesRefcount) {
    BlockStore store = makeStore();
    initSlot(store, BlockId{0});
    EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 0u);
    BlockHandle h = store.createHandle(BlockId{0});
    EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 1u);
}

TEST(BlockHandle, DestructorDecrementsRefcount) {
    BlockStore store = makeStore();
    initSlot(store, BlockId{0});
    {
        BlockHandle h = store.createHandle(BlockId{0});
        EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 1u);
    }
    EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 0u);
}

TEST(BlockHandle, MultipleHandlesStackRefcount) {
    BlockStore store = makeStore();
    initSlot(store, BlockId{0});
    auto h1 = store.createHandle(BlockId{0});
    auto h2 = store.createHandle(BlockId{0});
    EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 2u);
}

TEST(BlockHandle, MoveTransfersOwnership) {
    BlockStore store = makeStore();
    initSlot(store, BlockId{0});
    BlockHandle h1 = store.createHandle(BlockId{0});
    EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 1u);

    BlockHandle h2 = std::move(h1);
    EXPECT_FALSE(h1.valid());
    EXPECT_TRUE(h2.valid());
    EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 1u);  // no net change
}

TEST(BlockHandle, MoveAssignmentDecrementsOldAndTransfers) {
    BlockStore store = makeStore();
    initSlot(store, BlockId{0});
    initSlot(store, BlockId{1});

    BlockHandle h1 = store.createHandle(BlockId{0});
    BlockHandle h2 = store.createHandle(BlockId{1});
    EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 1u);
    EXPECT_EQ(store.get(BlockId{1}).refcount.load(), 1u);

    h2 = std::move(h1);
    EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 1u);  // h2 now holds block 0
    EXPECT_EQ(store.get(BlockId{1}).refcount.load(), 0u);  // block 1 released
    EXPECT_FALSE(h1.valid());
}

TEST(BlockHandle, IdAccessor) {
    BlockStore store = makeStore();
    initSlot(store, BlockId{3});
    BlockHandle h = store.createHandle(BlockId{3});
    EXPECT_EQ(h.id(), BlockId{3});
}

TEST(BlockHandle, MetadataAccessor) {
    BlockStore store = makeStore();
    store.initialize(BlockId{0}, 7u, {11, 22}, 2, 8, Tier::Host, BlockState::Ready);
    BlockHandle h = store.createHandle(BlockId{0});
    EXPECT_EQ(h.metadata().generation, 7u);
    EXPECT_EQ(h.metadata().hash.lo, 11u);
    EXPECT_EQ(h.metadata().block_index, 2u);
}

TEST(BlockHandle, ValidReturnsFalseForMovedFrom) {
    BlockStore store = makeStore();
    initSlot(store, BlockId{0});
    BlockHandle h1 = store.createHandle(BlockId{0});
    BlockHandle h2 = std::move(h1);
    EXPECT_FALSE(h1.valid());
    EXPECT_TRUE(h2.valid());
}

TEST(BlockHandle, RefcountPreventsEviction) {
    BlockStore store = makeStore();
    initSlot(store, BlockId{0});
    BlockHandle h = store.createHandle(BlockId{0});
    EXPECT_GT(store.get(BlockId{0}).refcount.load(), 0u);
    // Eviction must check refcount == 0 before transitioning Ready → Evicting
    // This test confirms refcount is non-zero while handle lives
}
