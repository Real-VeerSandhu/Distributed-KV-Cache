#include "policy/policies/no_promotion.h"

namespace kvcache::policy {

PromotionDecision NoPromotionPolicy::decide(const PromotionContext& /*context*/,
                                             const BlockPolicyFeatures& /*block*/) {
    return PromotionDecision::Stay;
}

const char* NoPromotionPolicy::name() const noexcept { return "no_promotion"; }

}  // namespace kvcache::policy
