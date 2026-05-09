#include "controller/promotion_controller.h"

#include <vector>

#include "sim/event.h"

namespace kvcache::controller {

PromotionController::PromotionController(policy::PromotionPolicy& promotion_policy,
                                          tier::TierManager& tier_manager, BlockManager& manager,
                                          const prefix::PrefixIndex& index,
                                          sim::EventSink& events)
    : promotion_policy_(promotion_policy),
      tier_manager_(tier_manager),
      manager_(manager),
      events_(events),
      feature_builder_(manager_.store(), index) {}

PromotionResult PromotionController::runScan(uint64_t timestamp_ns) {
    uint32_t promoted = 0;

    const tier::TierStats gpu_stats = tier_manager_.stats(Tier::GpuSim);
    const policy::PromotionContext ctx{gpu_stats.used_blocks, gpu_stats.capacity_blocks,
                                       timestamp_ns};

    // Snapshot the HOST block IDs to avoid mutating the set during iteration.
    const std::vector<BlockId> candidates{
        tier_manager_.tierStore(Tier::Host).blockIds().begin(),
        tier_manager_.tierStore(Tier::Host).blockIds().end()};

    for (const BlockId id : candidates) {
        if (!tier_manager_.hasCapacity(Tier::GpuSim)) {
            break;
        }
        const policy::BlockPolicyFeatures features = feature_builder_.build(id, timestamp_ns);
        if (promotion_policy_.decide(ctx, features) != policy::PromotionDecision::Promote) {
            continue;
        }
        const auto result = tier_manager_.promote(id, Tier::GpuSim);
        if (result.status == tier::TierOpResult::Status::Success) {
            events_.record(sim::BlockPromoted{id, Tier::Host, Tier::GpuSim, timestamp_ns});
            ++promoted;
        }
    }

    return {promoted};
}

}  // namespace kvcache::controller
