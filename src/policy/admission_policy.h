#pragma once

#include "core/ids.h"
#include "policy/policy_features.h"

namespace kvcache::policy {

enum class AdmissionDecision { Admit, Reject };

struct AdmissionContext {
    RequestId request_id;
    CacheContextHash context_hash;
    uint32_t missing_blocks;
    uint64_t timestamp_ns;
};

class AdmissionPolicy {
public:
    virtual ~AdmissionPolicy() = default;

    AdmissionPolicy(const AdmissionPolicy&) = delete;
    AdmissionPolicy& operator=(const AdmissionPolicy&) = delete;

    [[nodiscard]] virtual AdmissionDecision decide(const AdmissionContext& context,
                                                   const BlockCandidateFeatures& candidate) = 0;

    [[nodiscard]] virtual const char* name() const noexcept = 0;

protected:
    AdmissionPolicy() = default;
};

}  // namespace kvcache::policy
