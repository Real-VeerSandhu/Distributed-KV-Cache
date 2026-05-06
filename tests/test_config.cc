#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "config/config.h"

namespace kvcache {

namespace {

const std::string VALID_JSON = R"({
  "node": { "node_id": 0, "name": "worker-0" },
  "cache": {
    "payload_mode": "metadata_only",
    "tiers": {
      "GPU_SIM": { "capacity_blocks": 512 },
      "HOST":    { "capacity_blocks": 8192 }
    }
  },
  "context": {
    "model_id": 1, "tokenizer_id": 1, "block_size": 16,
    "dtype": "BF16", "num_layers": 32, "num_kv_heads": 8, "head_dim": 128,
    "rope_config_hash": 0, "kv_layout_hash": 0
  },
  "policies": {
    "admission": "always_admit", "eviction": "lru",
    "tier_placement": "gpu_first", "promotion": "no_promotion",
    "demotion": "no_demotion", "routing": "random",
    "replica_selection": "first", "prefetch": "none", "latency": "constant"
  },
  "latency": {
    "model": "constant",
    "params": { "local_lookup_ns": 1000, "tier_access_ns": 5000,
                "migration_ns": 50000, "network_fetch_ns": 500000 }
  },
  "workload": { "kind": "shared_system_prompt", "seed": 42, "requests": 1000 },
  "output": {
    "events_path": "out/events.jsonl",
    "decisions_path": "out/decisions.jsonl",
    "stats_path": "out/stats.json"
  }
})";

}  // namespace

TEST(Config, LoadsNodeId) {
    const auto cfg = Config::fromJsonString(VALID_JSON);
    EXPECT_EQ(cfg.node.node_id, NodeId{0});
}

TEST(Config, LoadsNodeName) {
    const auto cfg = Config::fromJsonString(VALID_JSON);
    EXPECT_EQ(cfg.node.name, "worker-0");
}

TEST(Config, LoadsPayloadMode) {
    const auto cfg = Config::fromJsonString(VALID_JSON);
    EXPECT_EQ(cfg.cache.payload_mode, PayloadMode::MetadataOnly);
}

TEST(Config, LoadsTierCapacities) {
    const auto cfg = Config::fromJsonString(VALID_JSON);
    ASSERT_EQ(cfg.cache.tiers.count("GPU_SIM"), 1U);
    ASSERT_EQ(cfg.cache.tiers.count("HOST"), 1U);
    EXPECT_EQ(cfg.cache.tiers.at("GPU_SIM").capacity_blocks, 512U);
    EXPECT_EQ(cfg.cache.tiers.at("HOST").capacity_blocks, 8192U);
}

TEST(Config, LoadsCacheKeyContext) {
    const auto cfg = Config::fromJsonString(VALID_JSON);
    EXPECT_EQ(cfg.context.model_id, ModelId{1});
    EXPECT_EQ(cfg.context.tokenizer_id, TokenizerId{1});
    EXPECT_EQ(cfg.context.block_size, 16U);
    EXPECT_EQ(cfg.context.dtype, DType::BF16);
    EXPECT_EQ(cfg.context.num_layers, 32U);
    EXPECT_EQ(cfg.context.num_kv_heads, 8U);
    EXPECT_EQ(cfg.context.head_dim, 128U);
}

TEST(Config, LoadsPolicyNames) {
    const auto cfg = Config::fromJsonString(VALID_JSON);
    EXPECT_EQ(cfg.policies.admission, "always_admit");
    EXPECT_EQ(cfg.policies.eviction, "lru");
    EXPECT_EQ(cfg.policies.tier_placement, "gpu_first");
    EXPECT_EQ(cfg.policies.promotion, "no_promotion");
    EXPECT_EQ(cfg.policies.demotion, "no_demotion");
    EXPECT_EQ(cfg.policies.routing, "random");
    EXPECT_EQ(cfg.policies.replica_selection, "first");
    EXPECT_EQ(cfg.policies.prefetch, "none");
    EXPECT_EQ(cfg.policies.latency, "constant");
}

TEST(Config, LoadsLatencyParams) {
    const auto cfg = Config::fromJsonString(VALID_JSON);
    EXPECT_EQ(cfg.latency.model, "constant");
    EXPECT_EQ(cfg.latency.params.local_lookup_ns, 1000U);
    EXPECT_EQ(cfg.latency.params.tier_access_ns, 5000U);
    EXPECT_EQ(cfg.latency.params.migration_ns, 50000U);
    EXPECT_EQ(cfg.latency.params.network_fetch_ns, 500000U);
}

TEST(Config, LoadsWorkload) {
    const auto cfg = Config::fromJsonString(VALID_JSON);
    EXPECT_EQ(cfg.workload.kind, "shared_system_prompt");
    EXPECT_EQ(cfg.workload.seed, 42U);
    EXPECT_EQ(cfg.workload.requests, 1000U);
}

TEST(Config, LoadsOutputPaths) {
    const auto cfg = Config::fromJsonString(VALID_JSON);
    EXPECT_EQ(cfg.output.events_path, "out/events.jsonl");
    EXPECT_EQ(cfg.output.decisions_path, "out/decisions.jsonl");
    EXPECT_EQ(cfg.output.stats_path, "out/stats.json");
}

TEST(Config, ThrowsOnMissingTopLevelField) {
    const std::string json = R"({"node":{"node_id":0,"name":"w"}})";
    EXPECT_THROW([&] { return Config::fromJsonString(json); }(), std::runtime_error);
}

TEST(Config, ThrowsOnUnknownPayloadMode) {
    std::string bad = VALID_JSON;
    const auto pos = bad.find("metadata_only");
    bad.replace(pos, std::string("metadata_only").size(), "quantum_storage");
    EXPECT_THROW([&] { return Config::fromJsonString(bad); }(), std::runtime_error);
}

TEST(Config, ThrowsOnUnknownDtype) {
    std::string bad = VALID_JSON;
    const auto pos = bad.find("\"BF16\"");
    bad.replace(pos, std::string("\"BF16\"").size(), "\"XYZ\"");
    EXPECT_THROW([&] { return Config::fromJsonString(bad); }(), std::runtime_error);
}

TEST(Config, ThrowsOnInvalidJson) {
    EXPECT_THROW([&] { return Config::fromJsonString("{not valid json}"); }(), std::runtime_error);
}

TEST(Config, ThrowsOnMissingFile) {
    EXPECT_THROW([&] { return Config::fromFile("/tmp/does_not_exist_kvcache_test.json"); }(),
                 std::runtime_error);
}

TEST(Config, LoadsSmallPayloadMode) {
    std::string json = VALID_JSON;
    const auto pos = json.find("metadata_only");
    json.replace(pos, std::string("metadata_only").size(), "small");
    const auto cfg = Config::fromJsonString(json);
    EXPECT_EQ(cfg.cache.payload_mode, PayloadMode::Small);
}

TEST(Config, LoadsRealisticPayloadMode) {
    std::string json = VALID_JSON;
    const auto pos = json.find("metadata_only");
    json.replace(pos, std::string("metadata_only").size(), "realistic");
    const auto cfg = Config::fromJsonString(json);
    EXPECT_EQ(cfg.cache.payload_mode, PayloadMode::Realistic);
}

}  // namespace kvcache
