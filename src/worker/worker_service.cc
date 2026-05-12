#include "worker/worker_service.h"

namespace kvcache::worker {

WorkerService::WorkerService(LocalCache& cache, pipeline::RequestPipeline& pipeline) noexcept
    : cache_(cache), pipeline_(pipeline) {}

pipeline::RouteResult WorkerService::lookup(const pipeline::RouteRequest& req) {
    return pipeline_.run(req);
}

AdmitResult WorkerService::admit(const BlockCandidate& candidate) {
    return cache_.admitBlock(candidate);
}

ServeBlockResult WorkerService::serveBlockFetch(BlockId id, uint64_t generation) {
    return cache_.serveBlockFetch(id, generation);
}

CacheSnapshot WorkerService::snapshot() const { return cache_.snapshot(); }

}  // namespace kvcache::worker
