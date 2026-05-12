#include "pipeline/remote_block_fetcher.h"

#include "policy/latency_model.h"
#include "sim/event.h"

namespace kvcache::pipeline {

RemoteBlockFetcher::RemoteBlockFetcher(transport::Transport& transport,
                                        policy::LatencyModel& latency,
                                        sim::EventSink& events) noexcept
    : transport_(transport), latency_(latency), events_(events) {}

FetchRemoteResult RemoteBlockFetcher::fetch(const GlobalBlockRef& ref,
                                             ContentHash expected_hash) {
    events_.record(sim::RemoteFetchStarted{expected_hash, ref.node_id, 0});

    auto resp = transport_.fetchBlock(ref.node_id, ref.block_id, ref.generation);

    if (resp.status == transport::FetchBlockResponse::Status::NotFound) {
        events_.record(sim::RemoteFetchFailed{expected_hash, ref.node_id,
                                              FetchFailReason::NotFound, 0});
        return {FetchRemoteResult::Status::NotFound, {}, {}, ref};
    }
    if (resp.status == transport::FetchBlockResponse::Status::StaleGeneration) {
        events_.record(sim::RemoteFetchFailed{expected_hash, ref.node_id,
                                              FetchFailReason::StaleGeneration, 0});
        return {FetchRemoteResult::Status::StaleGeneration, {}, {}, ref};
    }
    if (resp.status == transport::FetchBlockResponse::Status::TransportError) {
        events_.record(sim::RemoteFetchFailed{expected_hash, ref.node_id,
                                              FetchFailReason::TransportError, 0});
        return {FetchRemoteResult::Status::TransportError, {}, {}, ref};
    }

    if (resp.hash != expected_hash) {
        events_.record(sim::RemoteFetchFailed{expected_hash, ref.node_id,
                                              FetchFailReason::HashMismatch, 0});
        return {FetchRemoteResult::Status::HashMismatch, {}, {}, ref};
    }

    const uint64_t latency_ns = latency_.networkFetchNs(
        {static_cast<uint64_t>(resp.payload.size()), 0});
    (void)latency_ns;

    events_.record(sim::RemoteFetchCompleted{expected_hash, ref.node_id,
                                             static_cast<uint64_t>(resp.payload.size()), 0});

    return {FetchRemoteResult::Status::Ok, std::move(resp.payload), resp.hash, ref};
}

}  // namespace kvcache::pipeline
