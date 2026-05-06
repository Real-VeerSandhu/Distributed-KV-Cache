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

add_kvcache_test(test_environment    test_environment.cc)
add_kvcache_test(test_ids            test_ids.cc)
add_kvcache_test(test_content_hash   test_content_hash.cc)
add_kvcache_test(test_clock          test_clock.cc)
add_kvcache_test(test_event_sink     test_event_sink.cc)
add_kvcache_test(test_config         test_config.cc)
add_kvcache_test(test_policy_registry test_policy_registry.cc)
