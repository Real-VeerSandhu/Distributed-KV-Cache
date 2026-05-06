#pragma once

#include "policy/prefetch_policy.h"

namespace kvcache::policy {

class NoPrefetchPolicy : public PrefetchPolicy {
public:
    NoPrefetchPolicy() = default;

    [[nodiscard]] std::vector<ContentHash> planPrefetch(const PrefetchContext& context) override;

    [[nodiscard]] const char* name() const noexcept override;
};

}  // namespace kvcache::policy
