#include "policy/policies/always_admit.h"

namespace kvcache::policy {

AdmissionDecision AlwaysAdmitPolicy::decide(const AdmissionContext& /*context*/,
                                             const BlockCandidateFeatures& /*candidate*/) {
    return AdmissionDecision::Admit;
}

const char* AlwaysAdmitPolicy::name() const noexcept { return "always_admit"; }

}  // namespace kvcache::policy
