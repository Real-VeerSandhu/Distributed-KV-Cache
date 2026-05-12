from kvcache.orchestrator import RunConfig

TITLE = "Eviction Policy Under Memory Pressure"
DESCRIPTION = (
    "Shared system prompt workload with a cache smaller than the working set. "
    "LRU evicts useful root prefix blocks; PrefixAware protects high-fanout blocks, "
    "keeping shared prefixes warm at the cost of cold leaf blocks."
)

CONFIGS = [
    RunConfig(
        routing="random",
        eviction="lru",
        workload="shared_system_prompt",
        num_workers=1,
        capacity_per_worker=8,
        num_requests=300,
    ),
    RunConfig(
        routing="random",
        eviction="prefix_aware",
        workload="shared_system_prompt",
        num_workers=1,
        capacity_per_worker=8,
        num_requests=300,
    ),
]
