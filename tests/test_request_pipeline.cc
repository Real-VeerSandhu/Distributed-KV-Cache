#include <gtest/gtest.h>

#include <numeric>

#include "coordinator/coordinator_client.h"
#include "coordinator/coordinator_service.h"
#include "coordinator/global_index.h"
#include "coordinator/inproc_coordinator_client.h"
#include "coordinator/stale_reference_cleaner.h"
#include "coordinator/worker_directory.h"
#include "core/block_candidate.h"
#include "core/cache_key.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "core/ids.h"
#include "core/local_cache.h"
#include "pipeline/admission_stage.h"
#include "pipeline/lookup_stage.h"
#include "pipeline/remote_block_fetcher.h"
#include "pipeline/remote_resolution_stage.h"
#include "pipeline/request_pipeline.h"
#include "pipeline/route_types.h"
#include "policy/policies/constant_latency.h"
#include "policy/policies/first_replica.h"
#include "sim/event_sink.h"
#include "transport/inproc_transport.h"
#include "worker/worker_service.h"

using namespace kvcache;
using namespace kvcache::coordinator;
using namespace kvcache::pipeline;
using namespace kvcache::transport;
using namespace kvcache::worker;
using namespace kvcache::sim;
using namespace kvcache::policy;

namespace {

constexpr uint32_t BS = 4;
constexpr uint32_t CAPACITY = 64;
constexpr NodeId NODE_A{1};
constexpr NodeId NODE_B{2};

CacheKeyContext makeCtx() {
    return CacheKeyContext{ModelId{1}, TokenizerId{1}, BS, DType::BF16, 32, 8, 128, 0, 0};
}

BlockCandidate makeCandidate(const CacheKeyContext& ctx, uint32_t bi,
                              const std::vector<TokenId>& all_toks) {
    const CacheContextHash h = computeContextHash(ctx);
    const ContentHash ch = computeContentHash(h, bi, static_cast<uint16_t>(BS),
                                               Span<const TokenId>{all_toks.data() + bi * BS, BS});
    std::vector<TokenId> toks(all_toks.begin(),
                               all_toks.begin() + static_cast<ptrdiff_t>((bi + 1) * BS));
    return {ctx, bi, std::move(toks), ch, {}, BlockOrigin::LocallyComputed, std::nullopt};
}

RouteRequest makeRequest(RequestId rid, const CacheKeyContext& ctx,
                          const std::vector<TokenId>& tokens) {
    return {rid, ctx, tokens};
}

// Full rig for one worker with inproc transport and a shared coordinator.
struct WorkerRig {
    FakeClock clock;
    InMemoryEventSink events;
    LocalCache cache;
    ConstantLatencyModel latency;
    InprocTransport transport;
    RemoteBlockFetcher fetcher;
    FirstReplicaPolicy replica_policy;
    LookupStage lookup;
    AdmissionStage admission;

    explicit WorkerRig()
        : cache(CAPACITY, BS, clock),
          latency(0, 0, 0, 0),
          fetcher(transport, latency, events),
          lookup(cache),
          admission(cache) {}
};

struct TwoWorkerSetup {
    GlobalIndex global_idx;
    WorkerDirectory dir;
    StaleReferenceCleaner cleaner;
    CoordinatorService coord_svc;
    InprocCoordinatorClient coord_a;
    InprocCoordinatorClient coord_b;

    WorkerRig rig_a;
    WorkerRig rig_b;

    RemoteResolutionStage remote_a;
    RemoteResolutionStage remote_b;

    RequestPipeline pipeline_a;
    RequestPipeline pipeline_b;

    WorkerService service_a;
    WorkerService service_b;

    TwoWorkerSetup()
        : cleaner(global_idx),
          coord_svc(global_idx, dir, cleaner),
          coord_a(coord_svc),
          coord_b(coord_svc),
          remote_a(coord_a, rig_a.replica_policy, rig_a.fetcher, rig_a.events),
          remote_b(coord_b, rig_b.replica_policy, rig_b.fetcher, rig_b.events),
          pipeline_a(rig_a.lookup, remote_a, rig_a.admission, rig_a.events),
          pipeline_b(rig_b.lookup, remote_b, rig_b.admission, rig_b.events),
          service_a(rig_a.cache, pipeline_a),
          service_b(rig_b.cache, pipeline_b) {
        rig_a.transport.registerWorker(NODE_B, &service_b);
        rig_b.transport.registerWorker(NODE_A, &service_a);
        coord_svc.nodeJoin({NODE_A, ""});
        coord_svc.nodeJoin({NODE_B, ""});
    }
};

}  // namespace

// ── Local hit ────────────────────────────────────────────────────────────────

TEST(RequestPipeline, LocalHitRequiresNoCoordinator) {
    WorkerRig rig;
    NullCoordinatorClient null_coord;
    FirstReplicaPolicy replica;
    RemoteResolutionStage remote(null_coord, replica, rig.fetcher, rig.events);
    RequestPipeline pipeline(rig.lookup, remote, rig.admission, rig.events);

    const auto ctx = makeCtx();
    std::vector<TokenId> toks = {1, 2, 3, 4};

    auto cand = makeCandidate(ctx, 0, toks);
    rig.cache.admitBlock(cand);

    auto result = pipeline.run(makeRequest(RequestId{1}, ctx, toks));
    EXPECT_EQ(result.local_hit_blocks, 1u);
    EXPECT_EQ(result.remote_hit_blocks, 0u);
    EXPECT_EQ(result.matched_tokens, BS);
    EXPECT_EQ(result.reusable_blocks.size(), 1u);
}

TEST(RequestPipeline, MissReturnsEmptyResult) {
    WorkerRig rig;
    NullCoordinatorClient null_coord;
    FirstReplicaPolicy replica;
    RemoteResolutionStage remote(null_coord, replica, rig.fetcher, rig.events);
    RequestPipeline pipeline(rig.lookup, remote, rig.admission, rig.events);

    const auto ctx = makeCtx();
    std::vector<TokenId> toks = {1, 2, 3, 4};
    auto result = pipeline.run(makeRequest(RequestId{2}, ctx, toks));
    EXPECT_EQ(result.local_hit_blocks, 0u);
    EXPECT_EQ(result.remote_hit_blocks, 0u);
    EXPECT_EQ(result.matched_tokens, 0u);
}

// ── Remote hit ───────────────────────────────────────────────────────────────

TEST(RequestPipeline, RemoteHitFetchesAndAdmitsBlock) {
    TwoWorkerSetup setup;
    const auto ctx = makeCtx();
    std::vector<TokenId> toks = {1, 2, 3, 4};

    // Admit block 0 on worker B and announce to coordinator.
    auto cand = makeCandidate(ctx, 0, toks);
    const ContentHash hash = cand.hash;
    auto admitted = setup.rig_b.cache.admitBlock(cand);
    ASSERT_EQ(admitted.status, AdmitResult::Status::Admitted);
    const BlockId bid = admitted.handle->id();
    const uint64_t gen = admitted.handle->metadata().generation;
    admitted.handle.reset();

    setup.coord_svc.announce(hash, {NODE_B, bid, gen});

    // Worker A has no local block — should remote-fetch from B.
    auto result = setup.pipeline_a.run(makeRequest(RequestId{3}, ctx, toks));
    EXPECT_EQ(result.local_hit_blocks, 0u);
    EXPECT_EQ(result.remote_hit_blocks, 1u);
    EXPECT_EQ(result.matched_tokens, BS);
    ASSERT_EQ(result.reusable_blocks.size(), 1u);
}

TEST(RequestPipeline, RemoteHitAdmitsBlockLocallyForNextRequest) {
    TwoWorkerSetup setup;
    const auto ctx = makeCtx();
    std::vector<TokenId> toks = {1, 2, 3, 4};

    auto cand = makeCandidate(ctx, 0, toks);
    const ContentHash hash = cand.hash;
    auto admitted = setup.rig_b.cache.admitBlock(cand);
    const BlockId bid = admitted.handle->id();
    const uint64_t gen = admitted.handle->metadata().generation;
    admitted.handle.reset();
    setup.coord_svc.announce(hash, {NODE_B, bid, gen});

    // First request — remote fetch + local admission.
    setup.pipeline_a.run(makeRequest(RequestId{4}, ctx, toks));

    // Second request — should now be a pure local hit.
    NullCoordinatorClient null_coord;
    FirstReplicaPolicy replica;
    RemoteResolutionStage remote_no_coord(null_coord, replica, setup.rig_a.fetcher,
                                          setup.rig_a.events);
    RequestPipeline pipeline_local_only(setup.rig_a.lookup, remote_no_coord,
                                        setup.rig_a.admission, setup.rig_a.events);
    auto result2 = pipeline_local_only.run(makeRequest(RequestId{5}, ctx, toks));
    EXPECT_EQ(result2.local_hit_blocks, 1u);
    EXPECT_EQ(result2.remote_hit_blocks, 0u);
}

// ── Stale reference ───────────────────────────────────────────────────────────

TEST(RequestPipeline, StaleRefCausesEventsAndMiss) {
    TwoWorkerSetup setup;
    const auto ctx = makeCtx();
    std::vector<TokenId> toks = {1, 2, 3, 4};

    auto cand = makeCandidate(ctx, 0, toks);
    const ContentHash hash = cand.hash;
    auto admitted = setup.rig_b.cache.admitBlock(cand);
    const BlockId bid = admitted.handle->id();
    admitted.handle.reset();

    // Announce with wrong generation (stale ref).
    setup.coord_svc.announce(hash, {NODE_B, bid, 9999});

    auto result = setup.pipeline_a.run(makeRequest(RequestId{6}, ctx, toks));
    EXPECT_EQ(result.remote_hit_blocks, 0u);
    EXPECT_EQ(result.matched_tokens, 0u);

    const auto stale_events = setup.rig_a.events.eventsOfType<CoordinatorStaleRef>();
    EXPECT_GE(stale_events.size(), 1u);
}

// ── Stop at first missing block ───────────────────────────────────────────────

TEST(RequestPipeline, StopsAtFirstMissingBlock) {
    TwoWorkerSetup setup;
    const auto ctx = makeCtx();
    // 3 full blocks; worker B has block 0 but not block 1 → stop after block 0.
    std::vector<TokenId> toks = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    auto cand0 = makeCandidate(ctx, 0, toks);
    const ContentHash hash0 = cand0.hash;

    auto ad0 = setup.rig_b.cache.admitBlock(cand0);
    const BlockId bid0 = ad0.handle->id();
    const uint64_t gen0 = ad0.handle->metadata().generation;
    ad0.handle.reset();

    setup.coord_svc.announce(hash0, {NODE_B, bid0, gen0});
    // block 1 and 2 are NOT announced.

    auto result = setup.pipeline_a.run(makeRequest(RequestId{7}, ctx, toks));
    EXPECT_EQ(result.matched_tokens, BS);  // only block 0 reused
    EXPECT_EQ(result.miss_token_offset, BS);
    EXPECT_EQ(result.reusable_blocks.size(), 1u);
}

// ── Result preserves contiguous prefix ───────────────────────────────────────

TEST(RequestPipeline, ResultIsContiguousFromTokenZero) {
    WorkerRig rig;
    NullCoordinatorClient null_coord;
    FirstReplicaPolicy replica;
    RemoteResolutionStage remote(null_coord, replica, rig.fetcher, rig.events);
    RequestPipeline pipeline(rig.lookup, remote, rig.admission, rig.events);

    const auto ctx = makeCtx();
    std::vector<TokenId> toks = {1, 2, 3, 4, 5, 6, 7, 8};  // 2 full blocks

    auto c0 = makeCandidate(ctx, 0, toks);
    auto c1 = makeCandidate(ctx, 1, toks);
    rig.cache.admitBlock(c0);
    rig.cache.admitBlock(c1);

    auto result = pipeline.run(makeRequest(RequestId{8}, ctx, toks));
    EXPECT_EQ(result.matched_tokens, 2 * BS);
    EXPECT_EQ(result.reusable_blocks.size(), 2u);
    // Verify block indices are contiguous (0 then 1)
    EXPECT_EQ(result.reusable_blocks[0].metadata().block_index, 0u);
    EXPECT_EQ(result.reusable_blocks[1].metadata().block_index, 1u);
}
