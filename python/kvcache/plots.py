from __future__ import annotations

from pathlib import Path
from typing import List

import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

from kvcache.orchestrator import RunResult

_PALETTE = ["#2196F3", "#4CAF50", "#FF9800", "#E91E63"]


def plot_routing_comparison(results: List[RunResult], output: Path) -> None:
    labels = [r.config.routing for r in results]
    local = [r.local_hits for r in results]
    remote = [r.remote_hits for r in results]
    hit_rates = [r.prefix_hit_rate for r in results]

    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    fig.suptitle("Routing Policy Comparison", fontsize=14, fontweight="bold")

    x = list(range(len(labels)))

    axes[0].bar(x, local, label="Local", color=_PALETTE[0])
    axes[0].bar(x, remote, bottom=local, label="Remote", color=_PALETTE[2])
    axes[0].set_xticks(x)
    axes[0].set_xticklabels(labels)
    axes[0].set_title("Hit Distribution")
    axes[0].set_ylabel("Block Hits")
    axes[0].legend()

    axes[1].bar(labels, hit_rates, color=[_PALETTE[0], _PALETTE[1]])
    axes[1].set_title("Prefix Hit Rate")
    axes[1].set_ylabel("Hit Rate")
    axes[1].set_ylim(0, 1)
    axes[1].yaxis.set_major_formatter(mticker.PercentFormatter(xmax=1))

    remote_fracs = [r.remote_fraction for r in results]
    axes[2].bar(labels, remote_fracs, color=[_PALETTE[2], _PALETTE[3]])
    axes[2].set_title("Remote Fetch Fraction")
    axes[2].set_ylabel("Remote / Total Hits")
    axes[2].set_ylim(0, 1)
    axes[2].yaxis.set_major_formatter(mticker.PercentFormatter(xmax=1))

    plt.tight_layout()
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)


def plot_eviction_comparison(results: List[RunResult], output: Path) -> None:
    labels = [r.config.eviction for r in results]
    hit_rates = [r.prefix_hit_rate for r in results]
    evictions = [r.evictions for r in results]

    fig, axes = plt.subplots(1, 2, figsize=(10, 5))
    fig.suptitle("Eviction Policy Under Memory Pressure", fontsize=14, fontweight="bold")

    axes[0].bar(labels, hit_rates, color=[_PALETTE[0], _PALETTE[1]])
    axes[0].set_title("Prefix Hit Rate")
    axes[0].set_ylabel("Hit Rate")
    axes[0].set_ylim(0, 1)
    axes[0].yaxis.set_major_formatter(mticker.PercentFormatter(xmax=1))

    axes[1].bar(labels, evictions, color=[_PALETTE[2], _PALETTE[3]])
    axes[1].set_title("Eviction Count")
    axes[1].set_ylabel("Evictions")

    plt.tight_layout()
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)


def plot_per_worker(results: List[RunResult], output: Path) -> None:
    all_workers = [(r.config.routing, w) for r in results for w in r.workers]
    if not all_workers:
        return

    num = len(all_workers)
    fig, axes = plt.subplots(1, num, figsize=(5 * num, 5))
    if num == 1:
        axes = [axes]

    fig.suptitle("Per-Worker Stats", fontsize=14, fontweight="bold")

    for ax, (routing, w) in zip(axes, all_workers):
        ax.bar(["Local", "Remote"], [w.local_hits, w.remote_hits], color=[_PALETTE[0], _PALETTE[2]])
        ax.set_title(f"Worker {w.node_id}  [{routing}]")
        ax.set_ylabel("Hits")

    plt.tight_layout()
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
