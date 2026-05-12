#include "pipeline/lookup_stage.h"

#include "core/span.h"

namespace kvcache::pipeline {

LookupStage::LookupStage(LocalCache& cache) noexcept : cache_(cache) {}

LocalLookupOutcome LookupStage::run(const RouteRequest& req) {
    return cache_.lookupPrefix(req.context,
                                Span<const TokenId>{req.tokens.data(), req.tokens.size()});
}

}  // namespace kvcache::pipeline
