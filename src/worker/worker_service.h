#pragma once

#include "core/local_cache.h"
#include "pipeline/request_pipeline.h"
#include "pipeline/route_types.h"

namespace kvcache::worker {

class WorkerService {
public:
    WorkerService(LocalCache& cache, pipeline::RequestPipeline& pipeline) noexcept;

    [[nodiscard]] pipeline::RouteResult lookup(const pipeline::RouteRequest& req);
    [[nodiscard]] AdmitResult admit(const BlockCandidate& candidate);
    [[nodiscard]] ServeBlockResult serveBlockFetch(BlockId id, uint64_t generation);
    [[nodiscard]] CacheSnapshot snapshot() const;

private:
    LocalCache& cache_;
    pipeline::RequestPipeline& pipeline_;
};

}  // namespace kvcache::worker
