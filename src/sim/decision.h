#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/ids.h"

namespace kvcache::sim {

struct EvictionDecision {
    BlockId chosen_block;
    std::string policy_name;
    std::vector<BlockId> candidates;
    uint64_t timestamp_ns;
};

struct AdmissionRecord {
    bool admitted;
    std::string policy_name;
    ContentHash candidate_hash;
    uint64_t timestamp_ns;
};

using Decision = std::variant<EvictionDecision, AdmissionRecord>;

}  // namespace kvcache::sim
