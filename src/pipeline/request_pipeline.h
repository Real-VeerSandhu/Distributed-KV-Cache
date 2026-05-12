#pragma once

#include "pipeline/admission_stage.h"
#include "pipeline/lookup_stage.h"
#include "pipeline/remote_resolution_stage.h"
#include "pipeline/result_assembler.h"
#include "pipeline/route_types.h"
#include "sim/event_sink.h"

namespace kvcache::pipeline {

class RequestPipeline {
public:
    RequestPipeline(LookupStage& lookup, RemoteResolutionStage& remote, AdmissionStage& admission,
                    sim::EventSink& events) noexcept;

    [[nodiscard]] RouteResult run(const RouteRequest& req);

private:
    LookupStage& lookup_stage_;
    RemoteResolutionStage& remote_stage_;
    AdmissionStage& admission_stage_;
    ResultAssembler assembler_;
    sim::EventSink& events_;
};

}  // namespace kvcache::pipeline
