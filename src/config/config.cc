#include "config/config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace kvcache {

namespace {

PayloadMode parsePayloadMode(const std::string& s) {
    if (s == "metadata_only") return PayloadMode::MetadataOnly;
    if (s == "small") return PayloadMode::Small;
    if (s == "realistic") return PayloadMode::Realistic;
    throw std::runtime_error("config: unknown payload_mode: " + s);
}

DType parseDtype(const std::string& s) {
    if (s == "BF16") return DType::BF16;
    if (s == "FP16") return DType::FP16;
    if (s == "FP32") return DType::FP32;
    if (s == "INT8") return DType::INT8;
    throw std::runtime_error("config: unknown dtype: " + s);
}

NodeConfig parseNode(const nlohmann::json& j) {
    return NodeConfig{
        .node_id = static_cast<NodeId>(j.at("node_id").get<uint64_t>()),
        .name = j.at("name").get<std::string>(),
    };
}

CacheConfig parseCache(const nlohmann::json& j) {
    CacheConfig cfg;
    cfg.payload_mode = parsePayloadMode(j.at("payload_mode").get<std::string>());
    for (const auto& [tier_name, tier_j] : j.at("tiers").items()) {
        cfg.tiers[tier_name] = TierCapacityConfig{
            .capacity_blocks = tier_j.at("capacity_blocks").get<uint32_t>(),
        };
    }
    return cfg;
}

CacheKeyContext parseContext(const nlohmann::json& j) {
    return CacheKeyContext{
        .model_id = static_cast<ModelId>(j.at("model_id").get<uint64_t>()),
        .tokenizer_id = static_cast<TokenizerId>(j.at("tokenizer_id").get<uint64_t>()),
        .block_size = j.at("block_size").get<uint32_t>(),
        .dtype = parseDtype(j.at("dtype").get<std::string>()),
        .num_layers = j.at("num_layers").get<uint32_t>(),
        .num_kv_heads = j.at("num_kv_heads").get<uint32_t>(),
        .head_dim = j.at("head_dim").get<uint32_t>(),
        .rope_config_hash = j.at("rope_config_hash").get<uint64_t>(),
        .kv_layout_hash = j.at("kv_layout_hash").get<uint64_t>(),
    };
}

PolicyConfig parsePolicies(const nlohmann::json& j) {
    return PolicyConfig{
        .admission = j.at("admission").get<std::string>(),
        .eviction = j.at("eviction").get<std::string>(),
        .tier_placement = j.at("tier_placement").get<std::string>(),
        .promotion = j.at("promotion").get<std::string>(),
        .demotion = j.at("demotion").get<std::string>(),
        .routing = j.at("routing").get<std::string>(),
        .replica_selection = j.at("replica_selection").get<std::string>(),
        .prefetch = j.at("prefetch").get<std::string>(),
        .latency = j.at("latency").get<std::string>(),
    };
}

LatencyConfig parseLatency(const nlohmann::json& j) {
    const auto& params = j.at("params");
    return LatencyConfig{
        .model = j.at("model").get<std::string>(),
        .params = LatencyParams{
            .local_lookup_ns = params.value("local_lookup_ns", uint64_t{1000}),
            .tier_access_ns = params.value("tier_access_ns", uint64_t{5000}),
            .migration_ns = params.value("migration_ns", uint64_t{50000}),
            .network_fetch_ns = params.value("network_fetch_ns", uint64_t{500000}),
        },
    };
}

WorkloadConfig parseWorkload(const nlohmann::json& j) {
    return WorkloadConfig{
        .kind = j.at("kind").get<std::string>(),
        .seed = j.at("seed").get<uint32_t>(),
        .requests = j.at("requests").get<uint32_t>(),
    };
}

OutputConfig parseOutput(const nlohmann::json& j) {
    return OutputConfig{
        .events_path = j.at("events_path").get<std::string>(),
        .decisions_path = j.at("decisions_path").get<std::string>(),
        .stats_path = j.at("stats_path").get<std::string>(),
    };
}

Config parseConfig(const nlohmann::json& j) {
    return Config{
        .node = parseNode(j.at("node")),
        .cache = parseCache(j.at("cache")),
        .context = parseContext(j.at("context")),
        .policies = parsePolicies(j.at("policies")),
        .latency = parseLatency(j.at("latency")),
        .workload = parseWorkload(j.at("workload")),
        .output = parseOutput(j.at("output")),
    };
}

}  // namespace

Config Config::fromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("config: cannot open file: " + path);
    }
    try {
        return parseConfig(nlohmann::json::parse(f));
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("config: parse error in ") + path + ": " + e.what());
    }
}

Config Config::fromJsonString(const std::string& json_str) {
    try {
        return parseConfig(nlohmann::json::parse(json_str));
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("config: parse error: ") + e.what());
    }
}

}  // namespace kvcache
