from __future__ import annotations

import argparse
import sys
from pathlib import Path

from kvcache.orchestrator import Orchestrator
from kvcache.plots import plot_eviction_comparison, plot_routing_comparison
from kvcache.scenarios import eviction_pressure, prefix_reuse, routing_comparison
from kvcache.tui import launch

_DEFAULT_BENCH = Path(__file__).parent.parent / "build-test" / "apps" / "kvcache_bench"


def _find_bench() -> Path:
    candidates = [
        _DEFAULT_BENCH,
        Path(__file__).parent.parent / "build" / "apps" / "kvcache_bench",
    ]
    for p in candidates:
        if p.exists():
            return p
    raise FileNotFoundError(
        "kvcache_bench not found. Build the project first:\n"
        "  cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -Wno-dev\n"
        "  cmake --build build-test --parallel 4"
    )


def _cmd_tui(args: argparse.Namespace) -> None:
    bench = Path(args.bench) if args.bench else _find_bench()
    launch(bench)


def _cmd_run(args: argparse.Namespace) -> None:
    bench = Path(args.bench) if args.bench else _find_bench()
    orchestrator = Orchestrator(bench)

    scenarios = {
        "routing": routing_comparison,
        "eviction": eviction_pressure,
        "prefix": prefix_reuse,
    }

    scenario = scenarios[args.scenario]
    print(f"\n{scenario.TITLE}")
    print(scenario.DESCRIPTION)
    print()

    results = orchestrator.run_many(
        scenario.CONFIGS,
        on_run_complete=lambda r: print(
            f"  {r.config.label():<30}  "
            f"hit_rate={r.prefix_hit_rate:.1%}  "
            f"local={r.local_hits}  remote={r.remote_hits}  "
            f"remote_frac={r.remote_fraction:.1%}  evictions={r.evictions}"
        ),
    )

    if args.plot:
        out = Path(args.plot)
        if args.scenario == "routing":
            plot_routing_comparison(results, out)
        elif args.scenario == "eviction":
            plot_eviction_comparison(results, out)
        print(f"\nPlot saved to {out}")


def main() -> None:
    parser = argparse.ArgumentParser(prog="kvcache", description="kvcache Python orchestrator")
    parser.add_argument("--bench", metavar="PATH", help="path to kvcache_bench binary")
    sub = parser.add_subparsers(dest="command", required=True)

    tui_p = sub.add_parser("tui", help="launch interactive TUI")
    tui_p.set_defaults(func=_cmd_tui)

    run_p = sub.add_parser("run", help="run a scenario and print results")
    run_p.add_argument(
        "scenario",
        choices=["routing", "eviction", "prefix"],
        help="which scenario to run",
    )
    run_p.add_argument("--plot", metavar="PATH", help="save comparison plot to this path")
    run_p.set_defaults(func=_cmd_run)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
