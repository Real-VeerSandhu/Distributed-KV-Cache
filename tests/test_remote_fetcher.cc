#include <gtest/gtest.h>

#include "core/clock.h"
#include "core/content_hash.h"
#include "core/ids.h"
#include "core/local_cache.h"
#include "pipeline/remote_block_fetcher.h"
#include "policy/policies/constant_latency.h"
#include "sim/event_sink.h"
#include "transport/inproc_transport.h"
#include "worker/worker_service.h"

// Additional pipeline includes to build a full worker
#include "coordinator/coordinator_client.h"
#include "pipeline/admission_stage.h"
#include "pipeline/lookup_stage.h"
#include "pipeline/remote_resolution_stage.h"
#include "pipeline/request_pipeline.h"
#include "policy/policies/first_replica.h"

using namespace kvcache;
using namespace kvcache::pipeline;
using namespace kvcache::transport;
using namespace kvcache::worker;
using namespace kvcache::sim;
using namespace kvcache::policy;

namespace {

constexpr uint32_t BS = 4;
constexpr uint32_t CAPACITY = 32;
constexpr NodeId NODE_A{1};

CacheKeyContext makeCtx() {
    return CacheKeyContext{ModelId{1}, TokenizerId{1}, BS, DType::BF16, 32, 8, 128, 0, 0};
}

BlockCandidate makeCandidate(const CacheKeyContext& ctx, uint32_t bi,
                              std::initializer_list<TokenId> all_toks) {
    std::vector<TokenId> toks(all_toks);
    const CacheContextHash h = computeContextHash(ctx);
    const ContentHash ch = computeContentHash(h, bi, static_cast<uint16_t>(BS),
                                               Span<const TokenId>{toks.data() + bi * BS, BS});
    return {ctx, bi, toks, ch, {}, BlockOrigin::LocallyComputed, std::nullopt};
}

struct WorkerRig {
    FakeClock clock;
    InMemoryEventSink events;
    LocalCache cache;
    coordinator::NullCoordinatorClient coord_client;
    FirstReplicaPolicy replica_policy;
    ConstantLatencyModel latency;
    InprocTransport transport;
    RemoteBlockFetcher fetcher;
    LookupStage lookup;
    RemoteResolutionStage remote;
    AdmissionStage admission;
    RequestPipeline pipeline;
    WorkerService service;

    WorkerRig()
        : cache(CAPACITY, BS, clock),
          latency(0, 0, 0, 0),
          fetcher(transport, latency, events),
          lookup(cache),
          remote(coord_client, replica_policy, fetcher, events),
          admission(cache),
          pipeline(lookup, remote, admission, events),
          service(cache, pipeline) {}
};

}  // namespace

TEST(RemoteBlockFetcher, FetchFromKnownWorker) {
    WorkerRig source;
    WorkerRig requester;

    requester.transport.registerWorker(NODE_A, &source.service);

    const auto ctx = makeCtx();
    auto cand = makeCandidate(ctx, 0, {1, 2, 3, 4});
    const ContentHash expected_hash = cand.hash;

    auto admitted = source.cache.admitBlock(cand);
    ASSERT_EQ(admitted.status, AdmitResult::Status::Admitted);
    ASSERT_TRUE(admitted.handle.has_value());

    const BlockId bid = admitted.handle->id();
    const uint64_t gen = admitted.handle->metadata().generation;
    admitted.handle.reset();

    const GlobalBlockRef ref{NODE_A, bid, gen};
    auto fetch = requester.fetcher.fetch(ref, expected_hash);
    EXPECT_EQ(fetch.status, FetchRemoteResult::Status::Ok);
    EXPECT_EQ(fetch.hash, expected_hash);
}

TEST(RemoteBlockFetcher, FetchNotFoundForUnknownNode) {
    WorkerRig requester;
    const GlobalBlockRef ref{NodeId{99}, BlockId{0}, 1};
    const ContentHash hash{42, 0};
    auto fetch = requester.fetcher.fetch(ref, hash);
    EXPECT_EQ(fetch.status, FetchRemoteResult::Status::TransportError);
}

TEST(RemoteBlockFetcher, FetchStaleGenerationDetected) {
    WorkerRig source;
    WorkerRig requester;
    requester.transport.registerWorker(NODE_A, &source.service);

    const auto ctx = makeCtx();
    auto cand = makeCandidate(ctx, 0, {1, 2, 3, 4});
    const ContentHash expected_hash = cand.hash;

    auto admitted = source.cache.admitBlock(cand);
    ASSERT_EQ(admitted.status, AdmitResult::Status::Admitted);
    const BlockId bid = admitted.handle->id();
    admitted.handle.reset();

    // Use a wrong (stale) generation
    const GlobalBlockRef ref{NODE_A, bid, 9999};
    auto fetch = requester.fetcher.fetch(ref, expected_hash);
    EXPECT_EQ(fetch.status, FetchRemoteResult::Status::StaleGeneration);
}

TEST(RemoteBlockFetcher, FetchHashMismatchDetected) {
    WorkerRig source;
    WorkerRig requester;
    requester.transport.registerWorker(NODE_A, &source.service);

    const auto ctx = makeCtx();
    auto cand = makeCandidate(ctx, 0, {1, 2, 3, 4});

    auto admitted = source.cache.admitBlock(cand);
    ASSERT_EQ(admitted.status, AdmitResult::Status::Admitted);
    const BlockId bid = admitted.handle->id();
    const uint64_t gen = admitted.handle->metadata().generation;
    admitted.handle.reset();

    const GlobalBlockRef ref{NODE_A, bid, gen};
    const ContentHash wrong_hash{999, 888};
    auto fetch = requester.fetcher.fetch(ref, wrong_hash);
    EXPECT_EQ(fetch.status, FetchRemoteResult::Status::HashMismatch);
}

TEST(RemoteBlockFetcher, FetchEmitsRemoteFetchEvents) {
    WorkerRig source;
    WorkerRig requester;
    requester.transport.registerWorker(NODE_A, &source.service);

    const auto ctx = makeCtx();
    auto cand = makeCandidate(ctx, 0, {1, 2, 3, 4});
    const ContentHash expected_hash = cand.hash;

    auto admitted = source.cache.admitBlock(cand);
    const BlockId bid = admitted.handle->id();
    const uint64_t gen = admitted.handle->metadata().generation;
    admitted.handle.reset();

    const GlobalBlockRef ref{NODE_A, bid, gen};
    requester.fetcher.fetch(ref, expected_hash);

    const auto started = requester.events.eventsOfType<RemoteFetchStarted>();
    const auto completed = requester.events.eventsOfType<RemoteFetchCompleted>();
    EXPECT_EQ(started.size(), 1u);
    EXPECT_EQ(completed.size(), 1u);
    EXPECT_EQ(started[0].source_node, NODE_A);
}
