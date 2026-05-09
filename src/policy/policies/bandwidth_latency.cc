#include "policy/policies/bandwidth_latency.h"

#include "tier/tier.h"

namespace kvcache::policy {

BandwidthLatencyModel::BandwidthLatencyModel(uint64_t base_ns, double gpu_bytes_per_ns,
                                              double host_bytes_per_ns,
                                              double network_bytes_per_ns) noexcept
    : base_ns_(base_ns),
      gpu_bytes_per_ns_(gpu_bytes_per_ns),
      host_bytes_per_ns_(host_bytes_per_ns),
      network_bytes_per_ns_(network_bytes_per_ns) {}

uint64_t BandwidthLatencyModel::localLookupNs(const LookupCostInput& /*input*/) {
    return base_ns_;
}

uint64_t BandwidthLatencyModel::tierAccessNs(const TierAccessInput& input) {
    const double bw =
        (input.tier == Tier::GpuSim) ? gpu_bytes_per_ns_ : host_bytes_per_ns_;
    if (bw <= 0.0 || input.payload_bytes == 0) {
        return base_ns_;
    }
    return base_ns_ + static_cast<uint64_t>(
                          static_cast<double>(input.payload_bytes) / bw);
}

uint64_t BandwidthLatencyModel::migrationNs(const MigrationInput& input) {
    // Use the slower of the two tiers as the bottleneck bandwidth
    const double src_bw =
        (input.source == Tier::GpuSim) ? gpu_bytes_per_ns_ : host_bytes_per_ns_;
    const double dst_bw =
        (input.dest == Tier::GpuSim) ? gpu_bytes_per_ns_ : host_bytes_per_ns_;
    const double bw = (src_bw < dst_bw) ? src_bw : dst_bw;
    if (bw <= 0.0 || input.payload_bytes == 0) {
        return base_ns_;
    }
    return base_ns_ + static_cast<uint64_t>(
                          static_cast<double>(input.payload_bytes) / bw);
}

uint64_t BandwidthLatencyModel::networkFetchNs(const NetworkFetchInput& input) {
    uint64_t transfer_ns = 0;
    if (network_bytes_per_ns_ > 0.0 && input.payload_bytes > 0) {
        transfer_ns = static_cast<uint64_t>(
            static_cast<double>(input.payload_bytes) / network_bytes_per_ns_);
    }
    return base_ns_ + input.estimated_rtt_ns + transfer_ns;
}

const char* BandwidthLatencyModel::name() const noexcept { return "bandwidth"; }

}  // namespace kvcache::policy
