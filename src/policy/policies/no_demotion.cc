#include "policy/policies/no_demotion.h"

namespace kvcache::policy {

DemotionDecision NoDemotionPolicy::decide(const DemotionContext& /*context*/,
                                           const BlockPolicyFeatures& /*candidate*/) {
    return DemotionDecision::Stay;
}

const char* NoDemotionPolicy::name() const noexcept { return "no_demotion"; }

}  // namespace kvcache::policy
