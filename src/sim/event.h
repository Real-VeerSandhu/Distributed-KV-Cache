#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "core/ids.h"
#include "tier/tier.h"

namespace kvcache::sim {

struct LookupHit {
    BlockId id;
    uint64_t timestamp_ns;
};

struct LookupMiss {
    CacheContextHash context_hash;
    uint32_t miss_token_offset;
    uint64_t timestamp_ns;
};

struct BlockAdmitted {
    BlockId id;
    CacheContextHash context_hash;
    Tier tier;
    BlockOrigin origin;
    uint64_t timestamp_ns;
};

struct BlockEvicted {
    BlockId id;
    std::string policy_name;
    uint64_t timestamp_ns;
};

struct BlockPromoted {
    BlockId id;
    Tier from_tier;
    Tier to_tier;
    uint64_t timestamp_ns;
};

struct BlockDemoted {
    BlockId id;
    Tier from_tier;
    Tier to_tier;
    uint64_t timestamp_ns;
};

struct RemoteFetchStarted {
    ContentHash hash;
    NodeId source_node;
    uint64_t timestamp_ns;
};

struct RemoteFetchCompleted {
    ContentHash hash;
    NodeId source_node;
    uint64_t bytes_fetched;
    uint64_t timestamp_ns;
};

struct RemoteFetchFailed {
    ContentHash hash;
    NodeId source_node;
    FetchFailReason reason;
    uint64_t timestamp_ns;
};

struct CoordinatorStaleRef {
    NodeId node;
    BlockId block_id;
    uint64_t generation;
    uint64_t timestamp_ns;
};

struct NonContiguousLocalBlocks {
    uint32_t gap_at_block_index;
    uint64_t timestamp_ns;
};

using Event = std::variant<LookupHit,
                           LookupMiss,
                           BlockAdmitted,
                           BlockEvicted,
                           BlockPromoted,
                           BlockDemoted,
                           RemoteFetchStarted,
                           RemoteFetchCompleted,
                           RemoteFetchFailed,
                           CoordinatorStaleRef,
                           NonContiguousLocalBlocks>;

}  // namespace kvcache::sim
