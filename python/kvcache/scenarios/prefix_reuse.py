from kvcache.orchestrator import RunConfig

TITLE = "Prefix Reuse Demonstration"
DESCRIPTION = (
    "Single worker with a generous cache. Hit rate climbs from zero toward the "
    "theoretical maximum as the shared prefix tree warms up across requests."
)

CONFIGS = [
    RunConfig(
        routing="random",
        eviction="lru",
        workload="shared_system_prompt",
        num_workers=1,
        capacity_per_worker=64,
        num_requests=200,
    ),
]
