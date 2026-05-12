#include "pipeline/result_assembler.h"

namespace kvcache::pipeline {

RouteResult ResultAssembler::assemble(const RouteRequest& req, LocalLookupOutcome&& local,
                                       RemoteResolutionOutcome&&,
                                       AdmissionOutcome&& admission) {
    RouteResult result;
    result.local_hit_blocks = static_cast<uint32_t>(local.matched_blocks.size());
    result.remote_hit_blocks = admission.admitted_count;

    for (auto& handle : local.matched_blocks) {
        result.reusable_blocks.push_back(std::move(handle));
    }
    for (auto& handle : admission.admitted_handles) {
        result.reusable_blocks.push_back(std::move(handle));
    }

    const uint32_t block_size = req.context.block_size;
    result.matched_tokens =
        static_cast<uint32_t>(result.reusable_blocks.size()) * block_size;
    result.miss_token_offset = result.matched_tokens;

    return result;
}

}  // namespace kvcache::pipeline
