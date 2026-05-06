#include "core/block_manager.h"

#include <gtest/gtest.h>

#include "core/clock.h"

using namespace kvcache;

namespace {
ContentHash hash(uint64_t lo = 1, uint64_t hi = 2) { return {lo, hi}; }
}  // namespace

TEST(BlockManager, CreateBlockAllocatesSlot) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto result = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second, 1u);  // first generation
}

TEST(BlockManager, CreateBlockExhaustedReturnsNullopt) {
    FakeClock clock;
    BlockManager mgr{2, clock};
    ASSERT_TRUE(mgr.createBlock(hash(), 0, 4, Tier::GpuSim).has_value());
    ASSERT_TRUE(mgr.createBlock(hash(), 0, 4, Tier::GpuSim).has_value());
    EXPECT_FALSE(mgr.createBlock(hash(), 0, 4, Tier::GpuSim).has_value());
}

TEST(BlockManager, AcquireReturnsNulloptForNonReadyBlock) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto slot = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    // Block is in Admitting state — acquire should fail
    EXPECT_FALSE(mgr.acquire(slot->first).has_value());
}

TEST(BlockManager, AcquireSucceedsAfterTransitionToReady) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto slot = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    mgr.transitionState(slot->first, BlockState::Admitting, BlockState::Ready);
    auto handle = mgr.acquire(slot->first);
    ASSERT_TRUE(handle.has_value());
    EXPECT_EQ(handle->id(), slot->first);
}

TEST(BlockManager, AcquireIncreasesRefcount) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto slot = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    mgr.transitionState(slot->first, BlockState::Admitting, BlockState::Ready);
    {
        auto h = mgr.acquire(slot->first);
        ASSERT_TRUE(h.has_value());
        EXPECT_EQ(mgr.store().get(slot->first).refcount.load(), 1u);
    }
    EXPECT_EQ(mgr.store().get(slot->first).refcount.load(), 0u);
}

TEST(BlockManager, FetchLocalHitSucceeds) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto slot = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    mgr.transitionState(slot->first, BlockState::Admitting, BlockState::Ready);
    auto result = mgr.fetchLocal(slot->first, slot->second);
    EXPECT_EQ(result.status, FetchLocalResult::Status::Hit);
    ASSERT_TRUE(result.handle.has_value());
}

TEST(BlockManager, FetchLocalMissOnUnknownId) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto result = mgr.fetchLocal(BlockId{99}, 1u);
    EXPECT_EQ(result.status, FetchLocalResult::Status::NotFound);
}

TEST(BlockManager, FetchLocalStaleGeneration) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto slot = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    mgr.transitionState(slot->first, BlockState::Admitting, BlockState::Ready);
    auto result = mgr.fetchLocal(slot->first, slot->second + 1);
    EXPECT_EQ(result.status, FetchLocalResult::Status::StaleGeneration);
}

TEST(BlockManager, TransitionStateIllegalThrows) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto slot = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    // Admitting → Migrating is illegal
    EXPECT_THROW(
        [&] { mgr.transitionState(slot->first, BlockState::Admitting, BlockState::Migrating); }(),
        std::logic_error);
}

TEST(BlockManager, TransitionStateWrongFromThrows) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto slot = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    // actual state is Admitting, but we say from=Ready
    EXPECT_THROW(
        [&] { mgr.transitionState(slot->first, BlockState::Ready, BlockState::Evicting); }(),
        std::logic_error);
}

TEST(BlockManager, EvictBlockWithActiveHandleThrows) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto slot = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    mgr.transitionState(slot->first, BlockState::Admitting, BlockState::Ready);
    auto handle = mgr.acquire(slot->first);
    ASSERT_TRUE(handle.has_value());
    // refcount > 0 — eviction must be rejected
    EXPECT_THROW(
        [&] { mgr.transitionState(slot->first, BlockState::Ready, BlockState::Evicting); }(),
        std::logic_error);
}

TEST(BlockManager, FreeBlockAfterEvictingSucceeds) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto slot = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    mgr.transitionState(slot->first, BlockState::Admitting, BlockState::Ready);
    mgr.transitionState(slot->first, BlockState::Ready, BlockState::Evicting);
    EXPECT_NO_THROW(mgr.freeBlock(slot->first));
    // Slot should be available again
    EXPECT_FALSE(mgr.acquire(slot->first).has_value());
}

TEST(BlockManager, LegalTransitionsFullCycle) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    auto slot = mgr.createBlock(hash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    const BlockId id = slot->first;

    mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);
    mgr.transitionState(id, BlockState::Ready, BlockState::Migrating);
    mgr.transitionState(id, BlockState::Migrating, BlockState::Ready);
    mgr.transitionState(id, BlockState::Ready, BlockState::Evicting);
    mgr.freeBlock(id);
    EXPECT_EQ(mgr.store().get(id).generation, 1u);  // still gen 1 in store
}
