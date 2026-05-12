#include <gtest/gtest.h>

#include "coordinator/coordinator_client.h"
#include "coordinator/coordinator_service.h"
#include "coordinator/global_index.h"
#include "coordinator/inproc_coordinator_client.h"
#include "coordinator/stale_reference_cleaner.h"
#include "coordinator/worker_directory.h"
#include "core/ids.h"

using namespace kvcache;
using namespace kvcache::coordinator;

namespace {

constexpr NodeId NODE_A{1};
constexpr NodeId NODE_B{2};
constexpr BlockId BLOCK_0{0};
constexpr BlockId BLOCK_1{1};

ContentHash makeHash(uint64_t lo, uint64_t hi = 0) { return {lo, hi}; }

GlobalBlockRef makeRef(NodeId node, BlockId block, uint64_t gen = 1) {
    return {node, block, gen};
}

}  // namespace

// ── GlobalIndex ──────────────────────────────────────────────────────────────

TEST(GlobalIndex, EmptyQueryReturnsEmpty) {
    GlobalIndex idx;
    EXPECT_TRUE(idx.query(makeHash(42)).empty());
    EXPECT_EQ(idx.entryCount(), 0u);
}

TEST(GlobalIndex, AnnounceAndQuery) {
    GlobalIndex idx;
    idx.announce(makeHash(1), makeRef(NODE_A, BLOCK_0));
    auto refs = idx.query(makeHash(1));
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(refs[0].node_id, NODE_A);
    EXPECT_EQ(refs[0].block_id, BLOCK_0);
    EXPECT_EQ(refs[0].generation, 1u);
}

TEST(GlobalIndex, DuplicateAnnounceIsIdempotent) {
    GlobalIndex idx;
    idx.announce(makeHash(1), makeRef(NODE_A, BLOCK_0));
    idx.announce(makeHash(1), makeRef(NODE_A, BLOCK_0));
    EXPECT_EQ(idx.query(makeHash(1)).size(), 1u);
}

TEST(GlobalIndex, MultipleReplicasForSameHash) {
    GlobalIndex idx;
    idx.announce(makeHash(1), makeRef(NODE_A, BLOCK_0));
    idx.announce(makeHash(1), makeRef(NODE_B, BLOCK_1));
    EXPECT_EQ(idx.query(makeHash(1)).size(), 2u);
    EXPECT_EQ(idx.entryCount(), 2u);
}

TEST(GlobalIndex, InvalidateRemovesSpecificRef) {
    GlobalIndex idx;
    idx.announce(makeHash(1), makeRef(NODE_A, BLOCK_0, 1));
    idx.announce(makeHash(1), makeRef(NODE_B, BLOCK_1, 1));
    idx.invalidate(NODE_A, BLOCK_0, 1);
    auto refs = idx.query(makeHash(1));
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(refs[0].node_id, NODE_B);
}

TEST(GlobalIndex, InvalidateGenerationMismatchLeavesRefAlone) {
    GlobalIndex idx;
    idx.announce(makeHash(1), makeRef(NODE_A, BLOCK_0, 2));
    idx.invalidate(NODE_A, BLOCK_0, 1);  // wrong generation
    EXPECT_EQ(idx.query(makeHash(1)).size(), 1u);
}

TEST(GlobalIndex, RemoveNodeClearsAllItsRefs) {
    GlobalIndex idx;
    idx.announce(makeHash(1), makeRef(NODE_A, BLOCK_0));
    idx.announce(makeHash(2), makeRef(NODE_A, BLOCK_1));
    idx.announce(makeHash(1), makeRef(NODE_B, BLOCK_0));
    idx.removeNode(NODE_A);
    EXPECT_EQ(idx.query(makeHash(1)).size(), 1u);
    EXPECT_TRUE(idx.query(makeHash(2)).empty());
}

// ── WorkerDirectory ───────────────────────────────────────────────────────────

TEST(WorkerDirectory, EmptyDirectoryReturnsNullopt) {
    WorkerDirectory dir;
    EXPECT_FALSE(dir.lookup(NODE_A).has_value());
    EXPECT_EQ(dir.workerCount(), 0u);
}

TEST(WorkerDirectory, NodeJoinAndLookup) {
    WorkerDirectory dir;
    dir.nodeJoin({NODE_A, "addr-a"});
    auto info = dir.lookup(NODE_A);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->node_id, NODE_A);
    EXPECT_EQ(info->address, "addr-a");
    EXPECT_EQ(dir.workerCount(), 1u);
}

TEST(WorkerDirectory, NodeLeaveRemovesEntry) {
    WorkerDirectory dir;
    dir.nodeJoin({NODE_A, "addr-a"});
    dir.nodeLeave(NODE_A);
    EXPECT_FALSE(dir.lookup(NODE_A).has_value());
    EXPECT_EQ(dir.workerCount(), 0u);
}

TEST(WorkerDirectory, AllWorkersReturnsAll) {
    WorkerDirectory dir;
    dir.nodeJoin({NODE_A, "addr-a"});
    dir.nodeJoin({NODE_B, "addr-b"});
    EXPECT_EQ(dir.allWorkers().size(), 2u);
}

TEST(WorkerDirectory, JoinOverwritesExistingEntry) {
    WorkerDirectory dir;
    dir.nodeJoin({NODE_A, "addr-old"});
    dir.nodeJoin({NODE_A, "addr-new"});
    EXPECT_EQ(dir.workerCount(), 1u);
    EXPECT_EQ(dir.lookup(NODE_A)->address, "addr-new");
}

// ── StaleReferenceCleaner ─────────────────────────────────────────────────────

TEST(StaleReferenceCleaner, ReportStaleRemovesRef) {
    GlobalIndex idx;
    StaleReferenceCleaner cleaner{idx};
    idx.announce(makeHash(1), makeRef(NODE_A, BLOCK_0, 1));
    cleaner.reportStaleFetch(NODE_A, BLOCK_0, 1);
    EXPECT_TRUE(idx.query(makeHash(1)).empty());
}

TEST(StaleReferenceCleaner, CleanDeadNodeRemovesAllRefs) {
    GlobalIndex idx;
    StaleReferenceCleaner cleaner{idx};
    idx.announce(makeHash(1), makeRef(NODE_A, BLOCK_0));
    idx.announce(makeHash(2), makeRef(NODE_A, BLOCK_1));
    cleaner.cleanDeadNode(NODE_A);
    EXPECT_TRUE(idx.query(makeHash(1)).empty());
    EXPECT_TRUE(idx.query(makeHash(2)).empty());
}

// ── CoordinatorService ────────────────────────────────────────────────────────

TEST(CoordinatorService, AnnounceAndQuery) {
    GlobalIndex idx;
    WorkerDirectory dir;
    StaleReferenceCleaner cleaner{idx};
    CoordinatorService svc{idx, dir, cleaner};

    svc.announce(makeHash(1), makeRef(NODE_A, BLOCK_0));
    auto refs = svc.query(makeHash(1));
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(refs[0].node_id, NODE_A);
}

TEST(CoordinatorService, InvalidateRemovesRef) {
    GlobalIndex idx;
    WorkerDirectory dir;
    StaleReferenceCleaner cleaner{idx};
    CoordinatorService svc{idx, dir, cleaner};

    svc.announce(makeHash(1), makeRef(NODE_A, BLOCK_0, 3));
    svc.invalidate(NODE_A, BLOCK_0, 3);
    EXPECT_TRUE(svc.query(makeHash(1)).empty());
}

TEST(CoordinatorService, NodeLeaveRemovesAllRefs) {
    GlobalIndex idx;
    WorkerDirectory dir;
    StaleReferenceCleaner cleaner{idx};
    CoordinatorService svc{idx, dir, cleaner};

    svc.nodeJoin({NODE_A, "addr-a"});
    svc.announce(makeHash(1), makeRef(NODE_A, BLOCK_0));
    svc.nodeLeave(NODE_A);

    EXPECT_TRUE(svc.query(makeHash(1)).empty());
    EXPECT_FALSE(svc.directory().lookup(NODE_A).has_value());
}

// ── NullCoordinatorClient ─────────────────────────────────────────────────────

TEST(NullCoordinatorClient, QueryReturnsUnavailable) {
    NullCoordinatorClient client;
    auto result = client.query(makeHash(42));
    EXPECT_FALSE(result.available);
    EXPECT_TRUE(result.refs.empty());
}

TEST(NullCoordinatorClient, AnnounceAndInvalidateAreNoOps) {
    NullCoordinatorClient client;
    EXPECT_NO_THROW(client.announce(makeHash(1), makeRef(NODE_A, BLOCK_0)));
    EXPECT_NO_THROW(client.invalidate(NODE_A, BLOCK_0, 1));
}

// ── InprocCoordinatorClient ───────────────────────────────────────────────────

TEST(InprocCoordinatorClient, QueryDelegatesToService) {
    GlobalIndex idx;
    WorkerDirectory dir;
    StaleReferenceCleaner cleaner{idx};
    CoordinatorService svc{idx, dir, cleaner};
    InprocCoordinatorClient client{svc};

    svc.announce(makeHash(5), makeRef(NODE_A, BLOCK_0));
    auto result = client.query(makeHash(5));
    EXPECT_TRUE(result.available);
    ASSERT_EQ(result.refs.size(), 1u);
    EXPECT_EQ(result.refs[0].node_id, NODE_A);
}

TEST(InprocCoordinatorClient, AnnounceRoutsToService) {
    GlobalIndex idx;
    WorkerDirectory dir;
    StaleReferenceCleaner cleaner{idx};
    CoordinatorService svc{idx, dir, cleaner};
    InprocCoordinatorClient client{svc};

    client.announce(makeHash(7), makeRef(NODE_B, BLOCK_1));
    EXPECT_EQ(svc.query(makeHash(7)).size(), 1u);
}

TEST(InprocCoordinatorClient, InvalidateRoutesToService) {
    GlobalIndex idx;
    WorkerDirectory dir;
    StaleReferenceCleaner cleaner{idx};
    CoordinatorService svc{idx, dir, cleaner};
    InprocCoordinatorClient client{svc};

    client.announce(makeHash(9), makeRef(NODE_A, BLOCK_0, 2));
    client.invalidate(NODE_A, BLOCK_0, 2);
    EXPECT_TRUE(svc.query(makeHash(9)).empty());
}
