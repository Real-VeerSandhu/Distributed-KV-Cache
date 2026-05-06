#pragma once

#include "policy/policy_features.h"

namespace kvcache::policy {

enum class DemotionDecision { Demote, Stay };

struct DemotionContext {
    uint64_t gpu_sim_used_bytes;
    uint64_t gpu_sim_capacity_bytes;
    uint64_t timestamp_ns;
};

class DemotionPolicy {
public:
    virtual ~DemotionPolicy() = default;

    DemotionPolicy(const DemotionPolicy&) = delete;
    DemotionPolicy& operator=(const DemotionPolicy&) = delete;

    [[nodiscard]] virtual DemotionDecision decide(const DemotionContext& context,
                                                  const BlockPolicyFeatures& candidate) = 0;

    [[nodiscard]] virtual const char* name() const noexcept = 0;

protected:
    DemotionPolicy() = default;
};

}  // namespace kvcache::policy
