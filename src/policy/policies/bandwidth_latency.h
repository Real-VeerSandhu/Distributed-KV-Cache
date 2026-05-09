#pragma once

#include <cstdint>

#include "policy/latency_model.h"

namespace kvcache::policy {

// Latency = base_ns + payload_bytes / bytes_per_ns.
// Differentiates GPU_SIM and HOST bandwidth tiers.
class BandwidthLatencyModel : public LatencyModel {
public:
    explicit BandwidthLatencyModel(uint64_t base_ns = 100,
                                    double gpu_bytes_per_ns = 200.0,
                                    double host_bytes_per_ns = 20.0,
                                    double network_bytes_per_ns = 1.0) noexcept;

    [[nodiscard]] uint64_t localLookupNs(const LookupCostInput& input) override;
    [[nodiscard]] uint64_t tierAccessNs(const TierAccessInput& input) override;
    [[nodiscard]] uint64_t migrationNs(const MigrationInput& input) override;
    [[nodiscard]] uint64_t networkFetchNs(const NetworkFetchInput& input) override;

    [[nodiscard]] const char* name() const noexcept override;

private:
    uint64_t base_ns_;
    double gpu_bytes_per_ns_;
    double host_bytes_per_ns_;
    double network_bytes_per_ns_;
};

}  // namespace kvcache::policy
