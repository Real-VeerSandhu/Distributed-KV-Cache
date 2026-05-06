#pragma once

#include <cstdint>

#include "tier/tier.h"

namespace kvcache::policy {

struct LookupCostInput {
    uint32_t token_count;
    uint32_t matched_blocks;
};

struct TierAccessInput {
    Tier tier;
    uint64_t payload_bytes;
};

struct MigrationInput {
    Tier source;
    Tier dest;
    uint64_t payload_bytes;
};

struct NetworkFetchInput {
    uint64_t payload_bytes;
    uint64_t estimated_rtt_ns;
};

class LatencyModel {
public:
    virtual ~LatencyModel() = default;

    LatencyModel(const LatencyModel&) = delete;
    LatencyModel& operator=(const LatencyModel&) = delete;

    [[nodiscard]] virtual uint64_t localLookupNs(const LookupCostInput& input) = 0;
    [[nodiscard]] virtual uint64_t tierAccessNs(const TierAccessInput& input) = 0;
    [[nodiscard]] virtual uint64_t migrationNs(const MigrationInput& input) = 0;
    [[nodiscard]] virtual uint64_t networkFetchNs(const NetworkFetchInput& input) = 0;

    [[nodiscard]] virtual const char* name() const noexcept = 0;

protected:
    LatencyModel() = default;
};

}  // namespace kvcache::policy
