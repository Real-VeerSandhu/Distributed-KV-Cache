#pragma once

#include <vector>

#include "core/ids.h"

namespace kvcache::policy {

struct PrefetchContext {
    CacheContextHash context_hash;
    uint32_t last_matched_block_index;
    NodeId node_id;
    uint64_t timestamp_ns;
};

class PrefetchPolicy {
public:
    virtual ~PrefetchPolicy() = default;

    PrefetchPolicy(const PrefetchPolicy&) = delete;
    PrefetchPolicy& operator=(const PrefetchPolicy&) = delete;

    [[nodiscard]] virtual std::vector<ContentHash> planPrefetch(
        const PrefetchContext& context) = 0;

    [[nodiscard]] virtual const char* name() const noexcept = 0;

protected:
    PrefetchPolicy() = default;
};

}  // namespace kvcache::policy
