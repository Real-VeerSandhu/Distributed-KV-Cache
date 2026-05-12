from kvcache.orchestrator import RunConfig

TITLE = "Routing Policy Comparison"
DESCRIPTION = (
    "Same few-shot family workload on 2 workers. "
    "Hash routing pins each family to one worker; random routing scatters requests "
    "and forces cross-worker fetches."
)

CONFIGS = [
    RunConfig(
        routing="random",
        eviction="lru",
        workload="few_shot_families",
        num_workers=2,
        capacity_per_worker=12,
        num_requests=400,
    ),
    RunConfig(
        routing="hash_first_block",
        eviction="lru",
        workload="few_shot_families",
        num_workers=2,
        capacity_per_worker=12,
        num_requests=400,
    ),
]
