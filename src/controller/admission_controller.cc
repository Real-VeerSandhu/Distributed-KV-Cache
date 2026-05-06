#include "controller/admission_controller.h"

#include "core/content_hash.h"
#include "sim/decision.h"
#include "sim/event.h"

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
        .payload_bytes = 0,
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

    if (manager_.available() == 0) {
        const MakeSpaceRequest space_req{1, ctx.timestamp_ns};
        const auto space_result = eviction_controller_.makeSpace(space_req);
        if (space_result.status == EvictionResult::Status::InsufficientCandidates) {
            return {AdmitResult::Status::CapacityExceeded, std::nullopt};
        }
    }

    auto slot = manager_.createBlock(candidate.hash, candidate.block_index,
                                      static_cast<uint16_t>(block_size_), Tier::GpuSim);
    if (!slot) {
        return {AdmitResult::Status::CapacityExceeded, std::nullopt};
    }

    const auto [id, generation] = *slot;
    (void)generation;

    std::vector<BlockId> all_blocks = existing_blocks;
    all_blocks.push_back(id);
    lookup_engine_.insertFullBlocks(
        ctx_hash, Span<const TokenId>{candidate.tokens.data(), candidate.tokens.size()},
        Span<const BlockId>{all_blocks.data(), all_blocks.size()});

    manager_.transitionState(id, BlockState::Admitting, BlockState::Ready);

    events_.record(sim::BlockAdmitted{id, ctx_hash, Tier::GpuSim, candidate.origin,
                                       ctx.timestamp_ns});

    return {AdmitResult::Status::Admitted, manager_.acquire(id)};
}

}  // namespace kvcache::controller
