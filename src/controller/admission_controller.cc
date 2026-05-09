#include "controller/admission_controller.h"

#include "core/content_hash.h"
#include "policy/tier_placement_policy.h"
#include "sim/decision.h"
#include "sim/event.h"
#include "tier/tier_manager.h"

namespace kvcache::controller {

AdmissionController::AdmissionController(policy::AdmissionPolicy& admission_policy,
                                          EvictionController& eviction_controller,
                                          BlockManager& manager,
                                          prefix::PrefixLookupEngine& lookup_engine,
                                          uint32_t block_size, sim::EventSink& events,
                                          sim::DecisionLogger& decisions)
    : admission_policy_(admission_policy),
      eviction_controller_(eviction_controller),
      manager_(manager),
      lookup_engine_(lookup_engine),
      block_size_(block_size),
      events_(events),
      decisions_(decisions) {}

AdmissionController::AdmissionController(policy::AdmissionPolicy& admission_policy,
                                          policy::TierPlacementPolicy& placement_policy,
                                          tier::TierManager& tier_manager,
                                          EvictionController& eviction_controller,
                                          BlockManager& manager,
                                          prefix::PrefixLookupEngine& lookup_engine,
                                          uint32_t block_size, sim::EventSink& events,
                                          sim::DecisionLogger& decisions)
    : admission_policy_(admission_policy),
      placement_policy_(&placement_policy),
      tier_manager_(&tier_manager),
      eviction_controller_(eviction_controller),
      manager_(manager),
      lookup_engine_(lookup_engine),
      block_size_(block_size),
      events_(events),
      decisions_(decisions) {}

AdmitResult AdmissionController::admit(const BlockCandidate& candidate,
                                        const policy::AdmissionContext& ctx) {
    const size_t expected =
        (static_cast<size_t>(candidate.block_index) + 1) * static_cast<size_t>(block_size_);
    if (candidate.tokens.size() != expected) {
        return {AdmitResult::Status::InvalidBlock, std::nullopt};
    }

    const CacheContextHash ctx_hash = computeContextHash(candidate.context);

    const auto full_blocks = lookup_engine_.rawLookupBlocks(
        ctx_hash, Span<const TokenId>{candidate.tokens.data(), candidate.tokens.size()});
    if (full_blocks.size() == static_cast<size_t>(candidate.block_index) + 1) {
        return {AdmitResult::Status::AlreadyPresent, std::nullopt};
    }

    const auto existing_blocks = lookup_engine_.rawLookupBlocks(
        ctx_hash,
        Span<const TokenId>{candidate.tokens.data(),
                             static_cast<size_t>(candidate.block_index) * block_size_});
    if (existing_blocks.size() != static_cast<size_t>(candidate.block_index)) {
        return {AdmitResult::Status::InvalidBlock, std::nullopt};
    }

    const policy::BlockCandidateFeatures candidate_features{
        .hash = candidate.hash,
        .block_index = candidate.block_index,
        .token_count = static_cast<uint16_t>(block_size_),
        .payload_bytes = static_cast<uint64_t>(candidate.payload.size()),
        .origin = candidate.origin,
        .prefix_depth_blocks = candidate.block_index,
        .estimated_recompute_cost_ns = 0,
    };

    const auto decision = admission_policy_.decide(ctx, candidate_features);
    decisions_.log(sim::AdmissionRecord{decision == policy::AdmissionDecision::Admit,
                                           std::string{admission_policy_.name()},
                                           candidate.hash, ctx.timestamp_ns});

    if (decision == policy::AdmissionDecision::Reject) {
        return {AdmitResult::Status::Rejected, std::nullopt};
    }

    // Choose target tier via placement policy, or default to GpuSim.
    Tier target_tier = Tier::GpuSim;
    if (placement_policy_ != nullptr && tier_manager_ != nullptr) {
        const auto gpu_stats = tier_manager_->stats(Tier::GpuSim);
        const auto host_stats = tier_manager_->stats(Tier::Host);
        const policy::PlacementContext placement_ctx{
            .gpu_sim_used_blocks = gpu_stats.used_blocks,
            .gpu_sim_capacity_blocks = gpu_stats.capacity_blocks,
            .host_used_blocks = host_stats.used_blocks,
            .host_capacity_blocks = host_stats.capacity_blocks,
            .timestamp_ns = ctx.timestamp_ns,
        };
        target_tier = placement_policy_->chooseTier(placement_ctx, candidate_features);
    }

    // Ensure space exists — evict from the target tier if needed.
    const bool tier_full =
        (tier_manager_ != nullptr) && !tier_manager_->hasCapacity(target_tier);
    if (manager_.available() == 0 || tier_full) {
        const MakeSpaceRequest space_req{1, ctx.timestamp_ns, target_tier};
        const auto space_result = eviction_controller_.makeSpace(space_req);
        if (space_result.status == EvictionResult::Status::InsufficientCandidates) {
            return {AdmitResult::Status::CapacityExceeded, std::nullopt};
        }
    }

    auto slot = manager_.createBlock(candidate.hash, candidate.block_index,
                                      static_cast<uint16_t>(block_size_), target_tier);
    if (!slot) {
        return {AdmitResult::Status::CapacityExceeded, std::nullopt};
    }

    const auto [id, generation] = *slot;
    (void)generation;

    // Place payload in tier when TierManager is available.
    if (tier_manager_ != nullptr) {
        const Span<const std::byte> payload_span{candidate.payload.data(),
                                                   candidate.payload.size()};
        const auto place_result = tier_manager_->place(id, target_tier, payload_span);
        if (place_result.status == tier::TierOpResult::Status::CapacityExceeded) {
            manager_.freeBlock(id);
            return {AdmitResult::Status::CapacityExceeded, std::nullopt};
        }
    }

    std::vector<BlockId> all_blocks = existing_blocks;
    all_blocks.push_back(id);
    lookup_engine_.insertFullBlocks(
        ctx_hash, Span<const TokenId>{candidate.tokens.data(), candidate.tokens.size()},
        Span<const BlockId>{all_blocks.data(), all_blocks.size()});

    manager_.transitionState(id, BlockState::Admitting, BlockState::Ready);

    events_.record(sim::BlockAdmitted{id, ctx_hash, target_tier, candidate.origin,
                                       ctx.timestamp_ns});

    return {AdmitResult::Status::Admitted, manager_.acquire(id)};
}

}  // namespace kvcache::controller
