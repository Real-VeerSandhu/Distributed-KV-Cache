#include "pipeline/remote_resolution_stage.h"

#include "core/content_hash.h"
#include "core/span.h"
#include "sim/event.h"

namespace kvcache::pipeline {

RemoteResolutionStage::RemoteResolutionStage(coordinator::CoordinatorClient& coordinator,
                                              policy::ReplicaSelectionPolicy& replica_policy,
                                              RemoteBlockFetcher& fetcher,
                                              sim::EventSink& events) noexcept
    : coordinator_(coordinator),
      replica_policy_(replica_policy),
      fetcher_(fetcher),
      events_(events) {}

RemoteResolutionOutcome RemoteResolutionStage::run(const RouteRequest& req,
                                                    const LocalLookupOutcome& local) {
    const auto missing = planner_.plan(req, local);
    if (missing.empty()) return {};

    RemoteResolutionOutcome outcome;
    const uint32_t block_size = req.context.block_size;

    for (const auto& mb : missing) {
        auto query_result = coordinator_.query(mb.hash);
        if (!query_result.available || query_result.refs.empty()) break;

        const policy::ReplicaSelectionContext ctx{mb.hash, NodeId{0}, 0};
        auto chosen = replica_policy_.chooseReplica(
            ctx, Span<const GlobalBlockRef>{query_result.refs.data(), query_result.refs.size()});
        if (!chosen.has_value()) break;

        auto fetch = fetcher_.fetch(*chosen, mb.hash);

        if (fetch.status == FetchRemoteResult::Status::StaleGeneration) {
            coordinator_.invalidate(chosen->node_id, chosen->block_id, chosen->generation);
            events_.record(sim::CoordinatorStaleRef{chosen->node_id, chosen->block_id,
                                                    chosen->generation, 0});
            ++outcome.stale_ref_count;
            break;
        }
        if (fetch.status != FetchRemoteResult::Status::Ok) break;

        const uint32_t end_token = (mb.block_index + 1) * block_size;
        std::vector<TokenId> toks(req.tokens.begin(),
                                   req.tokens.begin() + static_cast<ptrdiff_t>(end_token));

        BlockCandidate candidate{req.context,       mb.block_index,
                                  std::move(toks),   fetch.hash,
                                  fetch.payload,     BlockOrigin::RemoteFetch,
                                  fetch.source_ref};

        outcome.fetched_blocks.push_back(std::move(candidate));
        ++outcome.remote_hit_count;
    }

    return outcome;
}

}  // namespace kvcache::pipeline
