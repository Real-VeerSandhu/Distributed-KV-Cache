#include "controller/eviction_controller.h"

#include <string>

#include "sim/decision.h"
#include "sim/event.h"
#include "tier/tier_manager.h"

namespace kvcache::controller {

EvictionController::EvictionController(policy::EvictionPolicy& eviction_policy,
                                        BlockManager& manager,
                                        prefix::PrefixLookupEngine& lookup_engine,
                                        const SafeCandidateFinder& candidate_finder,
                                        sim::EventSink& events, sim::DecisionLogger& decisions)
    : eviction_policy_(eviction_policy),
      manager_(manager),
      lookup_engine_(lookup_engine),
      candidate_finder_(candidate_finder),
      events_(events),
      decisions_(decisions),
      feature_builder_(manager_.store(), lookup_engine_.index()) {}

EvictionController::EvictionController(policy::EvictionPolicy& eviction_policy,
                                        BlockManager& manager,
                                        prefix::PrefixLookupEngine& lookup_engine,
                                        const SafeCandidateFinder& candidate_finder,
                                        tier::TierManager& tier_manager,
                                        sim::EventSink& events, sim::DecisionLogger& decisions)
    : eviction_policy_(eviction_policy),
      manager_(manager),
      lookup_engine_(lookup_engine),
      candidate_finder_(candidate_finder),
      tier_manager_(&tier_manager),
      events_(events),
      decisions_(decisions),
      feature_builder_(manager_.store(), lookup_engine_.index()) {}

EvictionResult EvictionController::makeSpace(const MakeSpaceRequest& request) {
    uint32_t evicted = 0;

    while (evicted < request.blocks_needed) {
        const std::vector<BlockId> all_candidates = candidate_finder_.find();
        if (all_candidates.empty()) {
            return {EvictionResult::Status::InsufficientCandidates, evicted};
        }

        // Filter to candidates in the requested tier.
        std::vector<BlockId> candidates;
        candidates.reserve(all_candidates.size());
        for (const BlockId id : all_candidates) {
            if (manager_.store().get(id).tier == request.tier) {
                candidates.push_back(id);
            }
        }
        if (candidates.empty()) {
            return {EvictionResult::Status::InsufficientCandidates, evicted};
        }

        std::vector<policy::BlockPolicyFeatures> features;
        features.reserve(candidates.size());
        for (const BlockId id : candidates) {
            features.push_back(feature_builder_.build(id, request.timestamp_ns));
        }

        const policy::EvictionContext ctx{request.tier, 0, request.timestamp_ns};
        const auto victim = eviction_policy_.chooseVictim(
            ctx, Span<const policy::BlockPolicyFeatures>{features.data(), features.size()});

        if (!victim) {
            return {EvictionResult::Status::InsufficientCandidates, evicted};
        }

        const BlockId victim_id = *victim;
        const std::string policy_name{eviction_policy_.name()};

        manager_.transitionState(victim_id, BlockState::Ready, BlockState::Evicting);
        lookup_engine_.removeBlock(victim_id);

        if (tier_manager_ != nullptr) {
            (void)tier_manager_->remove(victim_id);
        }

        manager_.freeBlock(victim_id);

        events_.record(sim::BlockEvicted{victim_id, policy_name, request.timestamp_ns});

        std::vector<BlockId> candidate_ids = candidates;
        decisions_.log(sim::EvictionDecision{victim_id, policy_name, std::move(candidate_ids),
                                              request.timestamp_ns});

        ++evicted;
    }

    return {EvictionResult::Status::Success, evicted};
}

}  // namespace kvcache::controller
