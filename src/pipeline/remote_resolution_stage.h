#pragma once

#include "coordinator/coordinator_client.h"
#include "core/local_cache.h"
#include "pipeline/missing_block_planner.h"
#include "pipeline/remote_block_fetcher.h"
#include "pipeline/route_types.h"
#include "policy/replica_selection_policy.h"
#include "sim/event_sink.h"

namespace kvcache::pipeline {

class RemoteResolutionStage {
public:
    RemoteResolutionStage(coordinator::CoordinatorClient& coordinator,
                           policy::ReplicaSelectionPolicy& replica_policy,
                           RemoteBlockFetcher& fetcher, sim::EventSink& events) noexcept;

    [[nodiscard]] RemoteResolutionOutcome run(const RouteRequest& req,
                                              const LocalLookupOutcome& local);

private:
    coordinator::CoordinatorClient& coordinator_;
    policy::ReplicaSelectionPolicy& replica_policy_;
    RemoteBlockFetcher& fetcher_;
    sim::EventSink& events_;
    MissingBlockPlanner planner_;
};

}  // namespace kvcache::pipeline
