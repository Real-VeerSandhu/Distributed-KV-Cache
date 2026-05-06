#pragma once

#include <cstdint>

#include "policy/latency_model.h"

namespace kvcache::policy {

class ConstantLatencyModel : public LatencyModel {
public:
    explicit ConstantLatencyModel(uint64_t local_lookup_cost_ns = 1'000,
                                  uint64_t tier_access_cost_ns = 5'000,
                                  uint64_t migration_cost_ns = 50'000,
                                  uint64_t network_fetch_cost_ns = 500'000) noexcept;

    [[nodiscard]] uint64_t localLookupNs(const LookupCostInput& input) override;
    [[nodiscard]] uint64_t tierAccessNs(const TierAccessInput& input) override;
    [[nodiscard]] uint64_t migrationNs(const MigrationInput& input) override;
    [[nodiscard]] uint64_t networkFetchNs(const NetworkFetchInput& input) override;

    [[nodiscard]] const char* name() const noexcept override;

private:
    uint64_t local_lookup_cost_ns_;
    uint64_t tier_access_cost_ns_;
    uint64_t migration_cost_ns_;
    uint64_t network_fetch_cost_ns_;
};

}  // namespace kvcache::policy
