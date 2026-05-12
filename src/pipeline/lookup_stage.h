#pragma once

#include "core/local_cache.h"
#include "pipeline/route_types.h"

namespace kvcache::pipeline {

class LookupStage {
public:
    explicit LookupStage(LocalCache& cache) noexcept;

    [[nodiscard]] LocalLookupOutcome run(const RouteRequest& req);

private:
    LocalCache& cache_;
};

}  // namespace kvcache::pipeline
