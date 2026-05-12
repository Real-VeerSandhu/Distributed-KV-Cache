#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "core/block_store.h"
#include "core/ids.h"
#include "core/span.h"
#include "policy/latency_model.h"
#include "tier/payload_store.h"
#include "tier/tier.h"
#include "tier/tier_store.h"

namespace kvcache::tier {

struct TierOpResult {
    enum class Status { Success, CapacityExceeded, BlockNotFound };
    Status status;
    uint64_t simulated_latency_ns{0};
};

// Executes physical placement and movement of blocks across tiers.
// Does not choose placement strategy — that belongs to TierPlacementPolicy.
class TierManager {
public:
    TierManager(std::unordered_map<Tier, TierStore> tiers, PayloadStore& payload_store,
                policy::LatencyModel& latency_model, BlockStore& block_store);

    // Place a newly admitted block into the target tier, storing payload bytes.
    [[nodiscard]] TierOpResult place(BlockId block_id, Tier target,
                                      Span<const std::byte> payload_bytes);

    // Move a block from its current tier to a faster target tier.
    [[nodiscard]] TierOpResult promote(BlockId block_id, Tier target);

    // Move a block from its current tier to a slower target tier.
    [[nodiscard]] TierOpResult demote(BlockId block_id, Tier target);

    // Remove a block from its tier and free its payload.
    [[nodiscard]] TierOpResult remove(BlockId block_id);

    [[nodiscard]] TierStats stats(Tier tier) const;
    [[nodiscard]] bool hasCapacity(Tier tier) const noexcept;
    [[nodiscard]] const TierStore& tierStore(Tier tier) const;
    [[nodiscard]] Span<const std::byte> getPayload(tier::PayloadRef ref) const;

private:
    std::unordered_map<Tier, TierStore> tiers_;
    PayloadStore& payload_store_;
    policy::LatencyModel& latency_model_;
    BlockStore& block_store_;
    // TODO(threading): per-tier mutex
};

}  // namespace kvcache::tier
