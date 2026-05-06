#pragma once

#include "policy/admission_policy.h"

namespace kvcache::policy {

class AlwaysAdmitPolicy : public AdmissionPolicy {
public:
    AlwaysAdmitPolicy() = default;

    [[nodiscard]] AdmissionDecision decide(const AdmissionContext& context,
                                           const BlockCandidateFeatures& candidate) override;

    [[nodiscard]] const char* name() const noexcept override;
};

}  // namespace kvcache::policy
