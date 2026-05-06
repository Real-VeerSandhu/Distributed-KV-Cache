#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "core/cache_key.h"
#include "core/ids.h"

namespace kvcache {

enum class PayloadMode { MetadataOnly, Small, Realistic };

struct NodeConfig {
    NodeId node_id;
    std::string name;
};

struct TierCapacityConfig {
    uint32_t capacity_blocks;
};

struct CacheConfig {
    PayloadMode payload_mode;
    std::unordered_map<std::string, TierCapacityConfig> tiers;
};

struct PolicyConfig {
    std::string admission;
    std::string eviction;
    std::string tier_placement;
    std::string promotion;
    std::string demotion;
    std::string routing;
    std::string replica_selection;
    std::string prefetch;
    std::string latency;
};

struct LatencyParams {
    uint64_t local_lookup_ns;
    uint64_t tier_access_ns;
    uint64_t migration_ns;
    uint64_t network_fetch_ns;
};

struct LatencyConfig {
    std::string model;
    LatencyParams params;
};

struct WorkloadConfig {
    std::string kind;
    uint32_t seed;
    uint32_t requests;
};

struct OutputConfig {
    std::string events_path;
    std::string decisions_path;
    std::string stats_path;
};

struct Config {
    NodeConfig node;
    CacheConfig cache;
    CacheKeyContext context;
    PolicyConfig policies;
    LatencyConfig latency;
    WorkloadConfig workload;
    OutputConfig output;

    [[nodiscard]] static Config fromFile(const std::string& path);
    [[nodiscard]] static Config fromJsonString(const std::string& json_str);
};

}  // namespace kvcache
