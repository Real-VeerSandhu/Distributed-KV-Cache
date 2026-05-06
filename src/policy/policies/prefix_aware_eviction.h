#pragma once

#include "policy/eviction_policy.h"

namespace kvcache::policy {

class PrefixAwareEvictionPolicy : public EvictionPolicy {
public:
    PrefixAwareEvictionPolicy() = default;

    [[nodiscard]] std::optional<BlockId> chooseVictim(
        const EvictionContext& context, Span<const BlockPolicyFeatures> candidates) override;

    [[nodiscard]] const char* name() const noexcept override;

private:
    static double score(const BlockPolicyFeatures& f);
};

}  // namespace kvcache::policy
