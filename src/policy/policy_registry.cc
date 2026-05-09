#include "policy/policy_registry.h"

#include <stdexcept>
#include <string>

#include "config/config.h"
#include "policy/policies/always_admit.h"
#include "policy/policies/bandwidth_latency.h"
#include "policy/policies/constant_latency.h"
#include "policy/policies/first_replica.h"
#include "policy/policies/gpu_first_placement.h"
#include "policy/policies/lru_eviction.h"
#include "policy/policies/no_demotion.h"
#include "policy/policies/no_prefetch.h"
#include "policy/policies/no_promotion.h"
#include "policy/policies/prefix_aware_eviction.h"
#include "policy/policies/random_routing.h"

namespace kvcache::policy {

namespace {

std::unique_ptr<AdmissionPolicy> makeAdmission(const std::string& name) {
    if (name == "always_admit") return std::make_unique<AlwaysAdmitPolicy>();
    throw std::runtime_error("policy_registry: unknown admission policy: " + name);
}

std::unique_ptr<EvictionPolicy> makeEviction(const std::string& name) {
    if (name == "lru") return std::make_unique<LruEvictionPolicy>();
    if (name == "prefix_aware") return std::make_unique<PrefixAwareEvictionPolicy>();
    throw std::runtime_error("policy_registry: unknown eviction policy: " + name);
}

std::unique_ptr<TierPlacementPolicy> makeTierPlacement(const std::string& name) {
    if (name == "gpu_first") return std::make_unique<GpuFirstPlacementPolicy>();
    throw std::runtime_error("policy_registry: unknown tier_placement policy: " + name);
}

std::unique_ptr<PromotionPolicy> makePromotion(const std::string& name) {
    if (name == "no_promotion") return std::make_unique<NoPromotionPolicy>();
    throw std::runtime_error("policy_registry: unknown promotion policy: " + name);
}

std::unique_ptr<DemotionPolicy> makeDemotion(const std::string& name) {
    if (name == "no_demotion") return std::make_unique<NoDemotionPolicy>();
    throw std::runtime_error("policy_registry: unknown demotion policy: " + name);
}

std::unique_ptr<RoutingPolicy> makeRouting(const std::string& name) {
    if (name == "random") return std::make_unique<RandomRoutingPolicy>();
    throw std::runtime_error("policy_registry: unknown routing policy: " + name);
}

std::unique_ptr<ReplicaSelectionPolicy> makeReplicaSelection(const std::string& name) {
    if (name == "first") return std::make_unique<FirstReplicaPolicy>();
    throw std::runtime_error("policy_registry: unknown replica_selection policy: " + name);
}

std::unique_ptr<PrefetchPolicy> makePrefetch(const std::string& name) {
    if (name == "none") return std::make_unique<NoPrefetchPolicy>();
    throw std::runtime_error("policy_registry: unknown prefetch policy: " + name);
}

std::unique_ptr<LatencyModel> makeLatency(const std::string& name,
                                           const LatencyParams& params) {
    if (name == "constant") {
        return std::make_unique<ConstantLatencyModel>(params.local_lookup_ns,
                                                      params.tier_access_ns,
                                                      params.migration_ns,
                                                      params.network_fetch_ns);
    }
    if (name == "bandwidth") {
        return std::make_unique<BandwidthLatencyModel>();
    }
    throw std::runtime_error("policy_registry: unknown latency model: " + name);
}

}  // namespace

PolicyRegistry PolicyRegistry::fromConfig(const PolicyConfig& cfg,
                                           const LatencyParams& latency_params) {
    return PolicyRegistry{
        .admission = makeAdmission(cfg.admission),
        .eviction = makeEviction(cfg.eviction),
        .tier_placement = makeTierPlacement(cfg.tier_placement),
        .promotion = makePromotion(cfg.promotion),
        .demotion = makeDemotion(cfg.demotion),
        .routing = makeRouting(cfg.routing),
        .replica_selection = makeReplicaSelection(cfg.replica_selection),
        .prefetch = makePrefetch(cfg.prefetch),
        .latency = makeLatency(cfg.latency, latency_params),
    };
}

}  // namespace kvcache::policy
