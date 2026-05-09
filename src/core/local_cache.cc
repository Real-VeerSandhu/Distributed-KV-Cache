#include "core/local_cache.h"

#include "policy/policies/always_admit.h"
#include "policy/policies/no_eviction.h"
#include "tier/tier_manager.h"

namespace kvcache {

LocalCache::LocalCache(uint32_t capacity, uint32_t block_size, Clock& clock)
    : owned_admission_policy_(std::make_unique<policy::AlwaysAdmitPolicy>()),
      owned_eviction_policy_(std::make_unique<policy::NoEvictionPolicy>()),
      owned_event_sink_(std::make_unique<sim::NullEventSink>()),
      owned_decision_logger_(std::make_unique<sim::NullDecisionLogger>()),
      block_size_(block_size),
      clock_(clock),
      manager_(capacity, clock),
      prefix_index_(block_size),
      lookup_engine_(prefix_index_, manager_),
      candidate_finder_(manager_.store()),
      eviction_controller_(*owned_eviction_policy_, manager_, lookup_engine_, candidate_finder_,
                           *owned_event_sink_, *owned_decision_logger_),
      admission_controller_(*owned_admission_policy_, eviction_controller_, manager_,
                            lookup_engine_, block_size_, *owned_event_sink_,
                            *owned_decision_logger_) {}

LocalCache::LocalCache(uint32_t capacity, uint32_t block_size, Clock& clock,
                       policy::AdmissionPolicy& admission_policy,
                       policy::EvictionPolicy& eviction_policy, sim::EventSink& events,
                       sim::DecisionLogger& decisions)
    : block_size_(block_size),
      clock_(clock),
      manager_(capacity, clock),
      prefix_index_(block_size),
      lookup_engine_(prefix_index_, manager_),
      candidate_finder_(manager_.store()),
      eviction_controller_(eviction_policy, manager_, lookup_engine_, candidate_finder_, events,
                           decisions),
      admission_controller_(admission_policy, eviction_controller_, manager_, lookup_engine_,
                            block_size_, events, decisions) {}

LocalCache::LocalCache(uint32_t capacity, uint32_t block_size, Clock& clock,
                       policy::AdmissionPolicy& admission_policy,
                       policy::EvictionPolicy& eviction_policy,
                       policy::TierPlacementPolicy& placement_policy,
                       tier::TierManager& tier_manager, sim::EventSink& events,
                       sim::DecisionLogger& decisions)
    : block_size_(block_size),
      clock_(clock),
      manager_(capacity, clock),
      prefix_index_(block_size),
      lookup_engine_(prefix_index_, manager_),
      candidate_finder_(manager_.store()),
      eviction_controller_(eviction_policy, manager_, lookup_engine_, candidate_finder_,
                           tier_manager, events, decisions),
      admission_controller_(admission_policy, placement_policy, tier_manager,
                            eviction_controller_, manager_, lookup_engine_, block_size_, events,
                            decisions) {}

LocalLookupOutcome LocalCache::lookupPrefix(const CacheKeyContext& ctx,
                                             Span<const TokenId> tokens) {
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    const uint32_t total_full =
        static_cast<uint32_t>(tokens.size() / static_cast<size_t>(block_size_));

    auto raw = lookup_engine_.lookup(ctx_hash, tokens, block_size_);

    LocalLookupOutcome outcome;
    outcome.matched_blocks = std::move(raw.handles);
    outcome.matched_tokens = raw.matched_tokens;
    outcome.miss_token_offset = raw.matched_tokens;
    outcome.context_hash = ctx_hash;
    outcome.total_full_blocks_in_request = total_full;
    return outcome;
}

AdmitResult LocalCache::admitBlock(const BlockCandidate& candidate) {
    const CacheContextHash ctx_hash = computeContextHash(candidate.context);
    const policy::AdmissionContext ctx{
        RequestId{0},
        ctx_hash,
        0,
        clock_.nowNs(),
    };
    auto result = admission_controller_.admit(candidate, ctx);

    AdmitResult out;
    switch (result.status) {
        case controller::AdmitResult::Status::Admitted:
            out.status = AdmitResult::Status::Admitted;
            break;
        case controller::AdmitResult::Status::AlreadyPresent:
            out.status = AdmitResult::Status::AlreadyPresent;
            break;
        case controller::AdmitResult::Status::CapacityExceeded:
            out.status = AdmitResult::Status::CapacityExceeded;
            break;
        case controller::AdmitResult::Status::InvalidBlock:
            out.status = AdmitResult::Status::InvalidBlock;
            break;
        case controller::AdmitResult::Status::Rejected:
            out.status = AdmitResult::Status::Rejected;
            break;
    }
    out.handle = std::move(result.handle);
    return out;
}

FetchLocalResult LocalCache::getBlock(BlockId id, uint64_t generation) {
    return manager_.fetchLocal(id, generation);
}

CacheSnapshot LocalCache::snapshot() const {
    const auto recs = manager_.store().records();
    uint32_t used = 0;
    uint32_t ready = 0;
    for (std::size_t i = 0; i < recs.size(); ++i) {
        if (recs[i].generation > 0 && recs[i].state != BlockState::Free) {
            ++used;
        }
        if (recs[i].state == BlockState::Ready) {
            ++ready;
        }
    }
    return {static_cast<uint32_t>(recs.size()), used, ready};
}

}  // namespace kvcache
