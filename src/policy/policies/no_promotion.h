#pragma once

#include "policy/promotion_policy.h"

namespace kvcache::policy {

class NoPromotionPolicy : public PromotionPolicy {
public:
    NoPromotionPolicy() = default;

    [[nodiscard]] PromotionDecision decide(const PromotionContext& context,
                                           const BlockPolicyFeatures& block) override;

    [[nodiscard]] const char* name() const noexcept override;
};

}  // namespace kvcache::policy
