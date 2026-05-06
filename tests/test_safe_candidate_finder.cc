#include "controller/safe_candidate_finder.h"

#include <gtest/gtest.h>

#include "core/block_manager.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "core/ids.h"

using namespace kvcache;
using namespace kvcache::controller;

namespace {

ContentHash dummyHash(uint64_t lo = 1) { return {lo, 0}; }

}  // namespace

TEST(SafeCandidateFinder, EmptyStoreReturnsNoCanidates) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    SafeCandidateFinder finder{mgr.store()};

    EXPECT_TRUE(finder.find().empty());
}

TEST(SafeCandidateFinder, ReadyBlockWithZeroRefcountIsCandidate) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    SafeCandidateFinder finder{mgr.store()};

    auto slot = mgr.createBlock(dummyHash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    const auto [id, gen] = *slot;
    mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);

    const auto candidates = finder.find();
    ASSERT_EQ(candidates.size(), 1u);
    EXPECT_EQ(candidates[0], id);
}

TEST(SafeCandidateFinder, BlockWithActiveHandleIsExcluded) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    SafeCandidateFinder finder{mgr.store()};

    auto slot = mgr.createBlock(dummyHash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    const auto [id, gen] = *slot;
    mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);

    auto handle = mgr.acquire(id);
    ASSERT_TRUE(handle.has_value());

    EXPECT_TRUE(finder.find().empty());
}

TEST(SafeCandidateFinder, NonReadyBlockIsExcluded) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    SafeCandidateFinder finder{mgr.store()};

    auto slot = mgr.createBlock(dummyHash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    (void)*slot;
    // Left in Admitting state — not Ready, should not be a candidate.

    EXPECT_TRUE(finder.find().empty());
}

TEST(SafeCandidateFinder, MultipleCandidatesAllReturned) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    SafeCandidateFinder finder{mgr.store()};

    for (uint32_t i = 0; i < 3; ++i) {
        auto slot = mgr.createBlock(dummyHash(i + 1), i, 4, Tier::GpuSim);
        ASSERT_TRUE(slot.has_value());
        const auto [id, gen] = *slot;
        (void)gen;
        mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);
    }

    const auto candidates = finder.find();
    ASSERT_EQ(candidates.size(), 3u);
}

TEST(SafeCandidateFinder, HandleDropRestoresCandidacy) {
    FakeClock clock;
    BlockManager mgr{4, clock};
    SafeCandidateFinder finder{mgr.store()};

    auto slot = mgr.createBlock(dummyHash(), 0, 4, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    const auto [id, gen] = *slot;
    mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);

    {
        auto handle = mgr.acquire(id);
        EXPECT_TRUE(finder.find().empty());
    }  // handle destroyed here

    const auto candidates = finder.find();
    ASSERT_EQ(candidates.size(), 1u);
    EXPECT_EQ(candidates[0], id);
}
