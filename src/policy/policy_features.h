#pragma once

#include <cstdint>

#include "core/ids.h"
#include "tier/tier.h"

namespace kvcache::policy {

struct BlockPolicyFeatures {
    BlockId id;
    ContentHash hash;
    Tier tier;
    uint64_t payload_bytes;
    uint64_t age_ns;
    uint64_t last_access_ns;
    uint64_t access_count;
    uint32_t prefix_depth_blocks;
    uint32_t prefix_child_count;
    uint32_t descendant_block_count;
    bool is_shared_prefix;
    bool is_remote_origin;
    uint64_t estimated_recompute_cost_ns;
    uint64_t estimated_refetch_cost_ns;
};

struct BlockCandidateFeatures {
    ContentHash hash;
    uint32_t block_index;
    uint16_t token_count;
    uint64_t payload_bytes;
    BlockOrigin origin;
    uint32_t prefix_depth_blocks;
    uint64_t estimated_recompute_cost_ns;
};

struct RequestPolicyFeatures {
    CacheContextHash context_hash;
    RequestId request_id;
    uint32_t token_count;
    uint32_t full_block_count;
    uint32_t already_matched_blocks;
    uint32_t missing_blocks;
    uint64_t estimated_prefill_cost_ns;
};

struct NodePolicyFeatures {
    NodeId node_id;
    uint64_t gpu_sim_used_bytes;
    uint64_t gpu_sim_capacity_bytes;
    uint64_t host_used_bytes;
    uint64_t host_capacity_bytes;
    uint64_t recent_local_hits;
    uint64_t recent_remote_hits;
    uint64_t recent_evictions;
    uint64_t estimated_queue_delay_ns;
};

}  // namespace kvcache::policy
