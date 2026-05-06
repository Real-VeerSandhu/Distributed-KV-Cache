add_executable(kvcache_bench kvcache_bench.cc)

target_link_libraries(kvcache_bench
    PRIVATE
        kvcache_core
)

set_project_warnings(kvcache_bench)
