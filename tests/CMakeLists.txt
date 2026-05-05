add_executable(test_environment test_environment.cc)

target_link_libraries(test_environment
  PRIVATE
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)

set_project_warnings(test_environment)

include(GoogleTest)
gtest_discover_tests(test_environment)
