#include "controller/eviction_controller.h"

#include <gtest/gtest.h>

#include "core/block_manager.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "policy/policies/lru_eviction.h"
#include "policy/policies/no_eviction.h"
#include "prefix/prefix_index.h"
#include "prefix/prefix_lookup_engine.h"
#include "sim/decision_logger.h"
#include "sim/event_sink.h"

using namespace kvcache;
using namespace kvcache::controller;
using namespace kvcache::policy;
using namespace kvcache::prefix;
using namespace kvcache::sim;

namespace {

constexpr uint32_t BS = 4;

ContentHash dummyHash(uint64_t lo = 1) { return {lo, 0}; }

struct Fixture {
    FakeClock clock;
    BlockManager mgr{8, clock};
    PrefixIndex index{BS};
    PrefixLookupEngine engine{index, mgr};
    SafeCandidateFinder finder{mgr.store()};
    InMemoryEventSink events;
    InMemoryDecisionLogger decisions;

    BlockId admitBlock(uint64_t hash_lo, uint32_t block_idx) {
        auto slot = mgr.createBlock(dummyHash(hash_lo), block_idx, BS, Tier::GpuSim);
        EXPECT_TRUE(slot.has_value());
        const auto [id, gen] = *slot;
        mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);
        return id;
    }

    EvictionController makeController(EvictionPolicy& policy) {
        return EvictionController{policy, mgr, engine, finder, events, decisions};
    }
};

}  // namespace

TEST(EvictionController, NoAvailableCandidatesReturnsInsufficient) {
    Fixture f;
    LruEvictionPolicy lru;
    auto ctrl = f.makeController(lru);

    const MakeSpaceRequest req{1, f.clock.nowNs()};
    const auto result = ctrl.makeSpace(req);

    EXPECT_EQ(result.status, EvictionResult::Status::InsufficientCandidates);
    EXPECT_EQ(result.evicted_count, 0u);
}

TEST(EvictionController, NoEvictionPolicyReturnsInsufficient) {
    Fixture f;
    const BlockId id = f.admitBlock(1, 0);
    (void)id;

    NoEvictionPolicy no_evict;
    auto ctrl = f.makeController(no_evict);

    const MakeSpaceRequest req{1, f.clock.nowNs()};
    const auto result = ctrl.makeSpace(req);

    EXPECT_EQ(result.status, EvictionResult::Status::InsufficientCandidates);
    EXPECT_EQ(result.evicted_count, 0u);
}

TEST(EvictionController, LruEvictsOneReadyBlock) {
    Fixture f;
    f.clock.advanceNs(1000);
    const BlockId id = f.admitBlock(1, 0);
    (void)id;

    LruEvictionPolicy lru;
    auto ctrl = f.makeController(lru);

    const MakeSpaceRequest req{1, f.clock.nowNs()};
    const auto result = ctrl.makeSpace(req);

    EXPECT_EQ(result.status, EvictionResult::Status::Success);
    EXPECT_EQ(result.evicted_count, 1u);
    EXPECT_EQ(f.mgr.store().get(id).state, BlockState::Free);
}

TEST(EvictionController, RefcountedBlockIsNotEvicted) {
    Fixture f;
    f.clock.advanceNs(1000);
    const BlockId id = f.admitBlock(1, 0);

    auto handle = f.mgr.acquire(id);
    ASSERT_TRUE(handle.has_value());

    LruEvictionPolicy lru;
    auto ctrl = f.makeController(lru);

    const MakeSpaceRequest req{1, f.clock.nowNs()};
    const auto result = ctrl.makeSpace(req);

    EXPECT_EQ(result.status, EvictionResult::Status::InsufficientCandidates);
    EXPECT_EQ(result.evicted_count, 0u);
    EXPECT_EQ(f.mgr.store().get(id).state, BlockState::Ready);
}

TEST(EvictionController, EvictionEmitsBlockEvictedEvent) {
    Fixture f;
    f.clock.advanceNs(1000);
    f.admitBlock(1, 0);

    LruEvictionPolicy lru;
    auto ctrl = f.makeController(lru);

    const MakeSpaceRequest req{1, f.clock.nowNs()};
    (void)ctrl.makeSpace(req);

    const auto evicted = f.events.eventsOfType<BlockEvicted>();
    ASSERT_EQ(evicted.size(), 1u);
    EXPECT_EQ(evicted[0].policy_name, "lru");
}

TEST(EvictionController, EvictionLogsDecision) {
    Fixture f;
    f.clock.advanceNs(1000);
    f.admitBlock(1, 0);

    LruEvictionPolicy lru;
    auto ctrl = f.makeController(lru);

    const MakeSpaceRequest req{1, f.clock.nowNs()};
    (void)ctrl.makeSpace(req);

    const auto logged = f.decisions.decisionsOfType<EvictionDecision>();
    ASSERT_EQ(logged.size(), 1u);
    EXPECT_EQ(logged[0].policy_name, "lru");
    EXPECT_FALSE(logged[0].candidates.empty());
}

TEST(EvictionController, LruChoosesOldestBlock) {
    Fixture f;

    f.clock.advanceNs(1000);
    const BlockId old_id = f.admitBlock(1, 0);

    f.clock.advanceNs(5000);
    f.mgr.markAccessed(old_id);  // bump old_id's last_access

    const BlockId new_id = f.admitBlock(2, 1);
    (void)new_id;

    // old_id was last accessed at t=6000, new_id was created at t=6000
    // Evict a second block to get a clear victim among the two
    // Reset: old_id was accessed more recently, so new_id should be evicted first
    // Actually: old_id last_access_ns=6000 (after markAccessed), new_id last_access_ns=6000 (created).
    // Tie-break: whichever comes first in iteration. Let's use a simpler setup.

    // Create three blocks, old_id accessed earliest:
    FakeClock clock2;
    BlockManager mgr2{4, clock2};
    PrefixIndex idx2{BS};
    PrefixLookupEngine eng2{idx2, mgr2};
    SafeCandidateFinder finder2{mgr2.store()};
    InMemoryEventSink events2;
    InMemoryDecisionLogger decisions2;

    clock2.advanceNs(100);
    auto slot_a = mgr2.createBlock(dummyHash(10), 0, BS, Tier::GpuSim);
    ASSERT_TRUE(slot_a.has_value());
    const auto [id_a, ga] = *slot_a;
    mgr2.transitionState(id_a, BlockState::Admitting, BlockState::Ready);

    clock2.advanceNs(1000);
    auto slot_b = mgr2.createBlock(dummyHash(11), 1, BS, Tier::GpuSim);
    ASSERT_TRUE(slot_b.has_value());
    const auto [id_b, gb] = *slot_b;
    mgr2.transitionState(id_b, BlockState::Admitting, BlockState::Ready);

    LruEvictionPolicy lru;
    EvictionController ctrl2{lru, mgr2, eng2, finder2, events2, decisions2};

    const MakeSpaceRequest req{1, clock2.nowNs()};
    const auto result = ctrl2.makeSpace(req);

    EXPECT_EQ(result.status, EvictionResult::Status::Success);
    const auto logged = decisions2.decisionsOfType<EvictionDecision>();
    ASSERT_EQ(logged.size(), 1u);
    EXPECT_EQ(logged[0].chosen_block, id_a);
}

TEST(EvictionController, MakeSpaceEvictsMultipleBlocks) {
    Fixture f;
    f.clock.advanceNs(1000);
    f.admitBlock(1, 0);
    f.admitBlock(2, 1);
    f.admitBlock(3, 2);

    LruEvictionPolicy lru;
    auto ctrl = f.makeController(lru);

    const MakeSpaceRequest req{2, f.clock.nowNs()};
    const auto result = ctrl.makeSpace(req);

    EXPECT_EQ(result.status, EvictionResult::Status::Success);
    EXPECT_EQ(result.evicted_count, 2u);
}

TEST(EvictionController, BlockFreedAfterEviction) {
    Fixture f;
    f.clock.advanceNs(1000);
    const BlockId id = f.admitBlock(1, 0);

    LruEvictionPolicy lru;
    auto ctrl = f.makeController(lru);

    (void)ctrl.makeSpace({1, f.clock.nowNs()});

    EXPECT_EQ(f.mgr.store().get(id).state, BlockState::Free);
    EXPECT_EQ(f.mgr.available(), 8u);
}
