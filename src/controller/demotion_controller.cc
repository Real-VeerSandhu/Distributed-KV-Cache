#include "controller/demotion_controller.h"

#include <vector>

#include "sim/event.h"

namespace kvcache::controller {

DemotionController::DemotionController(policy::DemotionPolicy& demotion_policy,
                                        tier::TierManager& tier_manager, BlockManager& manager,
                                        const prefix::PrefixIndex& index, sim::EventSink& events)
    : demotion_policy_(demotion_policy),
      tier_manager_(tier_manager),
      manager_(manager),
      events_(events),
      feature_builder_(manager_.store(), index) {}

DemotionResult DemotionController::runScan(uint64_t timestamp_ns) {
    uint32_t demoted = 0;

    const tier::TierStats gpu_stats = tier_manager_.stats(Tier::GpuSim);
    const policy::DemotionContext ctx{gpu_stats.used_blocks, gpu_stats.capacity_blocks,
                                      timestamp_ns};

    // Snapshot GPU_SIM block IDs to avoid mutating the set during iteration.
    const std::vector<BlockId> candidates{
        tier_manager_.tierStore(Tier::GpuSim).blockIds().begin(),
        tier_manager_.tierStore(Tier::GpuSim).blockIds().end()};

    for (const BlockId id : candidates) {
        if (!tier_manager_.hasCapacity(Tier::Host)) {
            break;
        }
        const policy::BlockPolicyFeatures features = feature_builder_.build(id, timestamp_ns);
        if (demotion_policy_.decide(ctx, features) != policy::DemotionDecision::Demote) {
            continue;
        }
        const auto result = tier_manager_.demote(id, Tier::Host);
        if (result.status == tier::TierOpResult::Status::Success) {
            events_.record(sim::BlockDemoted{id, Tier::GpuSim, Tier::Host, timestamp_ns});
            ++demoted;
        }
    }

    return {demoted};
}

}  // namespace kvcache::controller
