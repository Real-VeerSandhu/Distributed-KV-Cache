from __future__ import annotations

import json
import subprocess
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Dict, List, Optional

VALID_ROUTING = frozenset({"random", "hash_first_block"})
VALID_EVICTION = frozenset({"lru", "prefix_aware"})
VALID_WORKLOADS = frozenset({
    "shared_system_prompt",
    "few_shot_families",
    "adversarial_churn",
    "multi_turn_chat",
})


@dataclass(frozen=True)
class RunConfig:
    routing: str = "hash_first_block"
    eviction: str = "lru"
    workload: str = "shared_system_prompt"
    num_workers: int = 2
    capacity_per_worker: int = 12
    num_requests: int = 200
    seed: int = 42

    def __post_init__(self) -> None:
        if self.routing not in VALID_ROUTING:
            raise ValueError(f"routing must be one of {VALID_ROUTING}")
        if self.eviction not in VALID_EVICTION:
            raise ValueError(f"eviction must be one of {VALID_EVICTION}")
        if self.workload not in VALID_WORKLOADS:
            raise ValueError(f"workload must be one of {VALID_WORKLOADS}")

    def label(self) -> str:
        return f"{self.routing} / {self.eviction}"


@dataclass(frozen=True)
class WorkerStats:
    node_id: int
    local_hits: int
    remote_hits: int
    evictions: int
    admissions: int
    gpu_sim_used: int
    host_used: int

    @property
    def total_hits(self) -> int:
        return self.local_hits + self.remote_hits


@dataclass(frozen=True)
class RunResult:
    config: RunConfig
    local_hits: int
    remote_hits: int
    total_prefix_possible: int
    evictions: int
    admissions: int
    workers: List[WorkerStats] = field(default_factory=list)

    @property
    def total_hits(self) -> int:
        return self.local_hits + self.remote_hits

    @property
    def prefix_hit_rate(self) -> float:
        if self.total_prefix_possible == 0:
            return 0.0
        return self.total_hits / self.total_prefix_possible

    @property
    def local_hit_rate(self) -> float:
        if self.total_prefix_possible == 0:
            return 0.0
        return self.local_hits / self.total_prefix_possible

    @property
    def remote_fraction(self) -> float:
        if self.total_hits == 0:
            return 0.0
        return self.remote_hits / self.total_hits


class Orchestrator:
    def __init__(self, bench_binary: Path) -> None:
        if not bench_binary.exists():
            raise FileNotFoundError(f"bench binary not found: {bench_binary}")
        self._bench = bench_binary

    def run(
        self,
        config: RunConfig,
        *,
        on_event: Optional[Callable[[Dict], None]] = None,
    ) -> RunResult:
        stats_path = Path(tempfile.mktemp(suffix=".json"))
        args = self._build_args(config, stats_path)
        try:
            if on_event is not None:
                self._run_streaming(args, on_event)
            else:
                result = subprocess.run(args, capture_output=True, text=True)
                if result.returncode != 0:
                    raise RuntimeError(f"bench failed:\n{result.stderr}")
            return self._parse_result(config, stats_path)
        finally:
            stats_path.unlink(missing_ok=True)

    def run_many(
        self,
        configs: List[RunConfig],
        *,
        on_event: Optional[Callable[[Dict], None]] = None,
        on_run_complete: Optional[Callable[[RunResult], None]] = None,
    ) -> List[RunResult]:
        results = []
        for config in configs:
            result = self.run(config, on_event=on_event)
            results.append(result)
            if on_run_complete is not None:
                on_run_complete(result)
        return results

    def _build_args(self, config: RunConfig, stats_path: Path) -> List[str]:
        return [
            str(self._bench),
            "--routing", config.routing,
            "--eviction", config.eviction,
            "--workload", config.workload,
            "--workers", str(config.num_workers),
            "--capacity", str(config.capacity_per_worker),
            "--requests", str(config.num_requests),
            "--seed", str(config.seed),
            "--output-json", str(stats_path),
            "--stream-events",
        ]

    def _run_streaming(self, args: List[str], on_event: Callable[[Dict], None]) -> None:
        proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        assert proc.stdout is not None
        for line in proc.stdout:
            stripped = line.strip()
            if not stripped:
                continue
            try:
                on_event(json.loads(stripped))
            except json.JSONDecodeError:
                pass
        proc.wait()
        if proc.returncode != 0:
            stderr = proc.stderr.read() if proc.stderr else ""
            raise RuntimeError(f"bench failed:\n{stderr}")

    @staticmethod
    def _parse_result(config: RunConfig, stats_path: Path) -> RunResult:
        data = json.loads(stats_path.read_text())
        r = data["results"]
        workers = [
            WorkerStats(
                node_id=w["node_id"],
                local_hits=w["local_hits"],
                remote_hits=w["remote_hits"],
                evictions=w["evictions"],
                admissions=w["admissions"],
                gpu_sim_used=w.get("gpu_sim_used", 0),
                host_used=w.get("host_used", 0),
            )
            for w in data.get("workers", [])
        ]
        return RunResult(
            config=config,
            local_hits=r["local_hits"],
            remote_hits=r["remote_hits"],
            total_prefix_possible=r["total_prefix_possible"],
            evictions=r["evictions"],
            admissions=r["admissions"],
            workers=workers,
        )
