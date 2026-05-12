#pragma once

#include <vector>

#include "core/local_cache.h"
#include "pipeline/route_types.h"

namespace kvcache::pipeline {

// Computes content hashes for all missing full blocks after the local hit.
class MissingBlockPlanner {
public:
    MissingBlockPlanner() = default;

    [[nodiscard]] std::vector<MissingBlockInfo> plan(const RouteRequest& req,
                                                      const LocalLookupOutcome& local) const;
};

}  // namespace kvcache::pipeline
