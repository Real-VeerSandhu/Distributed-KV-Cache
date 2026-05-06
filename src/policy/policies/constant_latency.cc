#include "policy/policies/constant_latency.h"

namespace kvcache::policy {

ConstantLatencyModel::ConstantLatencyModel(uint64_t local_lookup_cost_ns,
                                            uint64_t tier_access_cost_ns,
                                            uint64_t migration_cost_ns,
                                            uint64_t network_fetch_cost_ns) noexcept
    : local_lookup_cost_ns_(local_lookup_cost_ns),
      tier_access_cost_ns_(tier_access_cost_ns),
      migration_cost_ns_(migration_cost_ns),
      network_fetch_cost_ns_(network_fetch_cost_ns) {}

uint64_t ConstantLatencyModel::localLookupNs(const LookupCostInput& /*input*/) {
    return local_lookup_cost_ns_;
}

uint64_t ConstantLatencyModel::tierAccessNs(const TierAccessInput& /*input*/) {
    return tier_access_cost_ns_;
}

uint64_t ConstantLatencyModel::migrationNs(const MigrationInput& /*input*/) {
    return migration_cost_ns_;
}

uint64_t ConstantLatencyModel::networkFetchNs(const NetworkFetchInput& /*input*/) {
    return network_fetch_cost_ns_;
}

const char* ConstantLatencyModel::name() const noexcept { return "constant"; }

}  // namespace kvcache::policy
