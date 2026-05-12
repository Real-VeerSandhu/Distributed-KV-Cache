#pragma once

#include "core/local_cache.h"
#include "core/span.h"
#include "pipeline/route_types.h"

namespace kvcache::pipeline {

class AdmissionStage {
public:
    explicit AdmissionStage(LocalCache& cache) noexcept;

    [[nodiscard]] AdmissionOutcome admitRemoteBlocks(const RouteRequest& req,
                                                      const RemoteResolutionOutcome& remote);

    [[nodiscard]] AdmissionOutcome admitComputedBlocks(const RouteRequest& req,
                                                        Span<const BlockCandidate> blocks);

private:
    LocalCache& cache_;
};

}  // namespace kvcache::pipeline
