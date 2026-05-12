#pragma once

#include "core/local_cache.h"
#include "pipeline/route_types.h"

namespace kvcache::pipeline {

class ResultAssembler {
public:
    ResultAssembler() = default;

    [[nodiscard]] RouteResult assemble(const RouteRequest& req, LocalLookupOutcome&& local,
                                        RemoteResolutionOutcome&& remote,
                                        AdmissionOutcome&& admission);
};

}  // namespace kvcache::pipeline
