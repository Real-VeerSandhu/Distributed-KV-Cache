include(GoogleTest)

function(add_kvcache_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name}
        PRIVATE
            kvcache_core
            GTest::gtest_main
    )
    set_project_warnings(${name})
    gtest_discover_tests(${name})
endfunction()

add_kvcache_test(test_environment         test_environment.cc)
add_kvcache_test(test_ids                 test_ids.cc)
add_kvcache_test(test_content_hash        test_content_hash.cc)
add_kvcache_test(test_clock               test_clock.cc)
add_kvcache_test(test_event_sink          test_event_sink.cc)
add_kvcache_test(test_config              test_config.cc)
add_kvcache_test(test_policy_registry     test_policy_registry.cc)
add_kvcache_test(test_block_pool          test_block_pool.cc)
add_kvcache_test(test_block_store         test_block_store.cc)
add_kvcache_test(test_block_handle        test_block_handle.cc)
add_kvcache_test(test_block_manager       test_block_manager.cc)
add_kvcache_test(test_prefix_index        test_prefix_index.cc)
add_kvcache_test(test_prefix_lookup_engine test_prefix_lookup_engine.cc)
add_kvcache_test(test_local_cache               test_local_cache.cc)
add_kvcache_test(test_scenario_phase1           test_scenario_phase1.cc)
add_kvcache_test(test_safe_candidate_finder     test_safe_candidate_finder.cc)
add_kvcache_test(test_eviction_controller       test_eviction_controller.cc)
add_kvcache_test(test_admission_controller      test_admission_controller.cc)
add_kvcache_test(test_scenario_phase2           test_scenario_phase2.cc)
