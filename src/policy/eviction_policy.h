#pragma once

#include <optional>

#include "core/ids.h"
#include "core/span.h"
#include "policy/policy_features.h"
#include "tier/tier.h"

namespace kvcache::policy {

struct EvictionContext {
    Tier tier;
    uint64_t needed_bytes;
    uint64_t timestamp_ns;
};

class EvictionPolicy {
public:
    virtual ~EvictionPolicy() = default;

    EvictionPolicy(const EvictionPolicy&) = delete;
    EvictionPolicy& operator=(const EvictionPolicy&) = delete;

    [[nodiscard]] virtual std::optional<BlockId> chooseVictim(
        const EvictionContext& context, Span<const BlockPolicyFeatures> candidates) = 0;

    [[nodiscard]] virtual const char* name() const noexcept = 0;

protected:
    EvictionPolicy() = default;
};

}  // namespace kvcache::policy
