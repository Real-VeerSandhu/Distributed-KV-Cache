add_library(kvcache_core STATIC
    core/dtype.cc
    core/clock.cc
    core/content_hash.cc
    core/block_pool.cc
    core/block_store.cc
    core/block_handle.cc
    core/block_manager.cc
    core/local_cache.cc
    tier/tier.cc
    sim/event_sink.cc
    config/config.cc
    policy/policy_registry.cc
    policy/policies/always_admit.cc
    policy/policies/lru_eviction.cc
    policy/policies/gpu_first_placement.cc
    policy/policies/no_promotion.cc
    policy/policies/no_demotion.cc
    policy/policies/random_routing.cc
    policy/policies/first_replica.cc
    policy/policies/no_prefetch.cc
    policy/policies/constant_latency.cc
    prefix/prefix_index.cc
    prefix/prefix_lookup_engine.cc
)

target_include_directories(kvcache_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kvcache_core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        xxhash_headers
)

set_project_warnings(kvcache_core)
