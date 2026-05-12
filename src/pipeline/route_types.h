#pragma once

#include <cstdint>
#include <vector>

#include "core/block_candidate.h"
#include "core/block_handle.h"
#include "core/cache_key.h"
#include "core/ids.h"

namespace kvcache::pipeline {

struct RouteRequest {
    RequestId request_id;
    CacheKeyContext context;
    std::vector<TokenId> tokens;
};

struct MissingBlockInfo {
    uint32_t block_index;
    ContentHash hash;
};

struct RemoteResolutionOutcome {
    std::vector<BlockCandidate> fetched_blocks;
    uint32_t remote_hit_count{0};
    uint32_t stale_ref_count{0};
};

struct AdmissionOutcome {
    std::vector<BlockHandle> admitted_handles;
    uint32_t admitted_count{0};
    uint32_t rejected_count{0};
};

struct RouteResult {
    std::vector<BlockHandle> reusable_blocks;
    uint32_t matched_tokens{0};
    uint32_t miss_token_offset{0};
    uint32_t local_hit_blocks{0};
    uint32_t remote_hit_blocks{0};
    uint64_t estimated_ttft_saved_ns{0};  // TODO(Phase 5): populate from latency model
};

}  // namespace kvcache::pipeline
