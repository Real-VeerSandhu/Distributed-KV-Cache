#include "pipeline/request_pipeline.h"

namespace kvcache::pipeline {

RequestPipeline::RequestPipeline(LookupStage& lookup, RemoteResolutionStage& remote,
                                  AdmissionStage& admission, sim::EventSink& events) noexcept
    : lookup_stage_(lookup),
      remote_stage_(remote),
      admission_stage_(admission),
      events_(events) {}

RouteResult RequestPipeline::run(const RouteRequest& req) {
    (void)events_;

    auto local = lookup_stage_.run(req);
    auto remote = remote_stage_.run(req, local);
    auto admitted = admission_stage_.admitRemoteBlocks(req, remote);

    return assembler_.assemble(req, std::move(local), std::move(remote), std::move(admitted));
}

}  // namespace kvcache::pipeline
