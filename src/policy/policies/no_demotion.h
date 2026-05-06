#pragma once

#include "policy/demotion_policy.h"

namespace kvcache::policy {

class NoDemotionPolicy : public DemotionPolicy {
public:
    NoDemotionPolicy() = default;

    [[nodiscard]] DemotionDecision decide(const DemotionContext& context,
                                          const BlockPolicyFeatures& candidate) override;

    [[nodiscard]] const char* name() const noexcept override;
};

}  // namespace kvcache::policy
