#pragma once

#include "policy/policy_features.h"

namespace kvcache::policy {

enum class PromotionDecision { Promote, Stay };

struct PromotionContext {
    uint64_t gpu_sim_used_bytes;
    uint64_t gpu_sim_capacity_bytes;
    uint64_t timestamp_ns;
};

class PromotionPolicy {
public:
    virtual ~PromotionPolicy() = default;

    PromotionPolicy(const PromotionPolicy&) = delete;
    PromotionPolicy& operator=(const PromotionPolicy&) = delete;

    [[nodiscard]] virtual PromotionDecision decide(const PromotionContext& context,
                                                   const BlockPolicyFeatures& block) = 0;

    [[nodiscard]] virtual const char* name() const noexcept = 0;

protected:
    PromotionPolicy() = default;
};

}  // namespace kvcache::policy
