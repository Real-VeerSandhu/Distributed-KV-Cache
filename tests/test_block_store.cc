#include "core/block_store.h"

#include <gtest/gtest.h>

#include "core/block_handle.h"

using namespace kvcache;

namespace {
ContentHash makeHash(uint64_t lo, uint64_t hi) { return {lo, hi}; }
}  // namespace

TEST(BlockStore, ExistsReturnsFalseForUninitializedSlot) {
    BlockStore store{4};
    EXPECT_FALSE(store.exists(BlockId{0}, 1u));
}

TEST(BlockStore, ExistsReturnsTrueAfterInitialize) {
    BlockStore store{4};
    store.initialize(BlockId{0}, 1u, makeHash(1, 2), 0, 4, Tier::GpuSim,
                     BlockState::Admitting);
    EXPECT_TRUE(store.exists(BlockId{0}, 1u));
}

TEST(BlockStore, ExistsReturnsFalseForWrongGeneration) {
    BlockStore store{4};
    store.initialize(BlockId{0}, 1u, makeHash(1, 2), 0, 4, Tier::GpuSim,
                     BlockState::Admitting);
    EXPECT_FALSE(store.exists(BlockId{0}, 2u));
}

TEST(BlockStore, ExistsReturnsFalseForOutOfRangeId) {
    BlockStore store{4};
    EXPECT_FALSE(store.exists(BlockId{99}, 1u));
}

TEST(BlockStore, GetReturnsCorrectRecord) {
    BlockStore store{4};
    store.initialize(BlockId{2}, 3u, makeHash(10, 20), 5, 16, Tier::Host,
                     BlockState::Ready);
    const auto& rec = store.get(BlockId{2});
    EXPECT_EQ(rec.id, BlockId{2});
    EXPECT_EQ(rec.generation, 3u);
    EXPECT_EQ(rec.hash.lo, 10u);
    EXPECT_EQ(rec.hash.hi, 20u);
    EXPECT_EQ(rec.block_index, 5u);
    EXPECT_EQ(rec.token_count, 16u);
    EXPECT_EQ(rec.tier, Tier::Host);
    EXPECT_EQ(rec.state, BlockState::Ready);
    EXPECT_EQ(rec.refcount.load(), 0u);
}

TEST(BlockStore, GetThrowsForOutOfRangeId) {
    BlockStore store{4};
    EXPECT_THROW([&] { static_cast<void>(store.get(BlockId{99})); }(), std::logic_error);
}

TEST(BlockStore, SetStateUpdatesState) {
    BlockStore store{4};
    store.initialize(BlockId{0}, 1u, makeHash(0, 0), 0, 4, Tier::GpuSim,
                     BlockState::Admitting);
    store.setState(BlockId{0}, BlockState::Ready);
    EXPECT_EQ(store.get(BlockId{0}).state, BlockState::Ready);
}

TEST(BlockStore, MarkAccessedUpdatesFields) {
    BlockStore store{4};
    store.initialize(BlockId{0}, 1u, makeHash(0, 0), 0, 4, Tier::GpuSim,
                     BlockState::Ready);
    store.markAccessed(BlockId{0}, 12345u);
    const auto& rec = store.get(BlockId{0});
    EXPECT_EQ(rec.last_access_ns, 12345u);
    EXPECT_EQ(rec.access_count, 1u);

    store.markAccessed(BlockId{0}, 99999u);
    EXPECT_EQ(store.get(BlockId{0}).access_count, 2u);
}

TEST(BlockStore, RecordsSpanCoversAllSlots) {
    BlockStore store{8};
    EXPECT_EQ(store.records().size(), 8u);
}

TEST(BlockStore, GenerationMatchCheck) {
    BlockStore store{4};
    store.initialize(BlockId{1}, 5u, makeHash(0, 0), 0, 4, Tier::GpuSim,
                     BlockState::Ready);
    EXPECT_TRUE(store.exists(BlockId{1}, 5u));
    EXPECT_FALSE(store.exists(BlockId{1}, 4u));
    EXPECT_FALSE(store.exists(BlockId{1}, 6u));
}

TEST(BlockStore, CreateHandleIncrementsRefcount) {
    BlockStore store{4};
    store.initialize(BlockId{0}, 1u, makeHash(0, 0), 0, 4, Tier::GpuSim,
                     BlockState::Ready);
    EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 0u);
    {
        BlockHandle h = store.createHandle(BlockId{0});
        EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 1u);
        EXPECT_EQ(h.id(), BlockId{0});
    }
    EXPECT_EQ(store.get(BlockId{0}).refcount.load(), 0u);
}
