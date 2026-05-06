#pragma once

#include "policy/eviction_policy.h"

namespace kvcache::policy {

class LruEvictionPolicy : public EvictionPolicy {
public:
    LruEvictionPolicy() = default;

    [[nodiscard]] std::optional<BlockId> chooseVictim(
        const EvictionContext& context, Span<const BlockPolicyFeatures> candidates) override;

    [[nodiscard]] const char* name() const noexcept override;
};

}  // namespace kvcache::policy
