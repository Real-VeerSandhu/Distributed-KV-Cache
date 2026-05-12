#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/ids.h"
#include "policy/latency_model.h"
#include "sim/event_sink.h"
#include "transport/transport.h"

namespace kvcache::pipeline {

struct FetchRemoteResult {
    enum class Status { Ok, NotFound, StaleGeneration, HashMismatch, TransportError };
    Status status{Status::TransportError};
    std::vector<std::byte> payload;
    ContentHash hash{};
    GlobalBlockRef source_ref{};
};

class RemoteBlockFetcher {
public:
    RemoteBlockFetcher(transport::Transport& transport, policy::LatencyModel& latency,
                       sim::EventSink& events) noexcept;

    [[nodiscard]] FetchRemoteResult fetch(const GlobalBlockRef& ref, ContentHash expected_hash);

private:
    transport::Transport& transport_;
    policy::LatencyModel& latency_;
    sim::EventSink& events_;
};

}  // namespace kvcache::pipeline
