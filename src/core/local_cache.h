#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "controller/admission_controller.h"
#include "controller/eviction_controller.h"
#include "controller/safe_candidate_finder.h"
#include "core/block_candidate.h"
#include "core/block_handle.h"
#include "core/block_manager.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "core/ids.h"
#include "core/span.h"
#include "policy/admission_policy.h"
#include "policy/eviction_policy.h"
#include "prefix/prefix_index.h"
#include "prefix/prefix_lookup_engine.h"
#include "sim/decision_logger.h"
#include "sim/event_sink.h"

namespace kvcache {

struct LocalLookupOutcome {
    std::vector<BlockHandle> matched_blocks;
    uint32_t matched_tokens{0};
    uint32_t miss_token_offset{0};
    CacheContextHash context_hash{0};
    uint32_t total_full_blocks_in_request{0};
};

struct AdmitResult {
    enum class Status { Admitted, AlreadyPresent, CapacityExceeded, InvalidBlock, Rejected };
    Status status;
    std::optional<BlockHandle> handle;
};

struct CacheSnapshot {
    uint32_t total_capacity_blocks{0};
    uint32_t used_blocks{0};
    uint32_t ready_blocks{0};
};

class LocalCache {
public:
    // Phase 1-compatible constructor: uses AlwaysAdmit, NoEviction, null sinks.
    LocalCache(uint32_t capacity, uint32_t block_size, Clock& clock);

    // Full Phase 2 constructor: caller supplies policies and observability.
    LocalCache(uint32_t capacity, uint32_t block_size, Clock& clock,
               policy::AdmissionPolicy& admission_policy,
               policy::EvictionPolicy& eviction_policy, sim::EventSink& events,
               sim::DecisionLogger& decisions);

    [[nodiscard]] LocalLookupOutcome lookupPrefix(const CacheKeyContext& ctx,
                                                   Span<const TokenId> tokens);

    [[nodiscard]] AdmitResult admitBlock(const BlockCandidate& candidate);

    [[nodiscard]] FetchLocalResult getBlock(BlockId id, uint64_t generation);

    [[nodiscard]] CacheSnapshot snapshot() const;

private:
    // Owned stubs — populated only by the simple constructor.
    std::unique_ptr<policy::AdmissionPolicy> owned_admission_policy_;
    std::unique_ptr<policy::EvictionPolicy> owned_eviction_policy_;
    std::unique_ptr<sim::EventSink> owned_event_sink_;
    std::unique_ptr<sim::DecisionLogger> owned_decision_logger_;

    uint32_t block_size_;
    Clock& clock_;
    BlockManager manager_;
    prefix::PrefixIndex prefix_index_;
    prefix::PrefixLookupEngine lookup_engine_;
    controller::SafeCandidateFinder candidate_finder_;
    controller::EvictionController eviction_controller_;
    controller::AdmissionController admission_controller_;
};

}  // namespace kvcache
