#include "pipeline/missing_block_planner.h"

#include "core/content_hash.h"

namespace kvcache::pipeline {

std::vector<MissingBlockInfo> MissingBlockPlanner::plan(const RouteRequest& req,
                                                         const LocalLookupOutcome& local) const {
    const uint32_t block_size = req.context.block_size;
    const uint32_t total_tokens = static_cast<uint32_t>(req.tokens.size());
    const uint32_t total_full_blocks = total_tokens / block_size;
    const uint32_t first_missing = static_cast<uint32_t>(local.matched_blocks.size());

    if (first_missing >= total_full_blocks) return {};

    const CacheContextHash ctx_hash = computeContextHash(req.context);
    std::vector<MissingBlockInfo> result;
    result.reserve(total_full_blocks - first_missing);

    for (uint32_t bi = first_missing; bi < total_full_blocks; ++bi) {
        const auto* block_start = req.tokens.data() + bi * block_size;
        const ContentHash hash = computeContentHash(
            ctx_hash, bi, static_cast<uint16_t>(block_size),
            Span<const TokenId>{block_start, block_size});
        result.push_back({bi, hash});
    }
    return result;
}

}  // namespace kvcache::pipeline
