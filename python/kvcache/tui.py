from __future__ import annotations

from pathlib import Path
from typing import List

from textual import on, work
from textual.app import App, ComposeResult
from textual.containers import Horizontal, Vertical
from textual.widgets import Button, DataTable, Footer, Header, Label, Log, ProgressBar

from kvcache.orchestrator import Orchestrator, RunConfig, RunResult
from kvcache.scenarios import eviction_pressure, prefix_reuse, routing_comparison

_SCENARIOS = {
    "routing": routing_comparison,
    "eviction": eviction_pressure,
    "prefix": prefix_reuse,
}

_RESULT_COLUMNS = (
    "Scenario",
    "Routing",
    "Eviction",
    "Workers",
    "Hit Rate",
    "Local Hits",
    "Remote Hits",
    "Remote %",
    "Evictions",
)


class KVCacheTUI(App):
    CSS = """
    Screen {
        layout: vertical;
        background: $surface;
    }

    #scenario-bar {
        height: 3;
        layout: horizontal;
        padding: 0 1;
        background: $panel;
        border-bottom: solid $primary;
    }

    #scenario-bar Button {
        margin: 0 1;
    }

    #status-bar {
        height: 1;
        padding: 0 1;
        background: $panel-darken-1;
    }

    #main {
        height: 1fr;
        layout: vertical;
    }

    #results {
        height: 1fr;
        border: solid $primary;
        margin: 1;
    }

    #events {
        height: 14;
        border: solid $accent;
        margin: 0 1 1 1;
    }
    """

    BINDINGS = [
        ("q", "quit", "Quit"),
        ("c", "clear_results", "Clear"),
    ]

    def __init__(self, orchestrator: Orchestrator) -> None:
        super().__init__()
        self._orchestrator = orchestrator
        self._running = False

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        yield Horizontal(
            Button("Routing Comparison", id="btn-routing", variant="primary"),
            Button("Eviction Pressure", id="btn-eviction", variant="primary"),
            Button("Prefix Reuse", id="btn-prefix", variant="primary"),
            id="scenario-bar",
        )
        yield Label("", id="status-bar")
        with Vertical(id="main"):
            yield DataTable(id="results", zebra_stripes=True)
            yield Log(id="events", max_lines=200, highlight=True)
        yield Footer()

    def on_mount(self) -> None:
        table = self.query_one("#results", DataTable)
        table.add_columns(*_RESULT_COLUMNS)

    @on(Button.Pressed, "#btn-routing")
    def _on_routing(self) -> None:
        self._start_scenario("routing")

    @on(Button.Pressed, "#btn-eviction")
    def _on_eviction(self) -> None:
        self._start_scenario("eviction")

    @on(Button.Pressed, "#btn-prefix")
    def _on_prefix(self) -> None:
        self._start_scenario("prefix")

    def _start_scenario(self, key: str) -> None:
        if self._running:
            return
        self._running = True
        scenario = _SCENARIOS[key]
        self._set_status(f"Running: {scenario.TITLE}")
        self._execute(key, scenario.CONFIGS)

    @work(thread=True)
    def _execute(self, scenario_key: str, configs: List[RunConfig]) -> None:
        scenario = _SCENARIOS[scenario_key]
        log = self.query_one("#events", Log)

        for config in configs:
            self.call_from_thread(
                log.write_line,
                f"[bold]▶ {scenario.TITLE}[/bold]  routing={config.routing}  "
                f"eviction={config.eviction}  workers={config.num_workers}  "
                f"requests={config.num_requests}",
            )

            def on_event(event: dict, _cfg: RunConfig = config) -> None:
                event_type = event.get("type", "")
                node = event.get("node_id", "?")
                detail = _format_event(event)
                self.call_from_thread(log.write_line, f"  [W{node}] {event_type}  {detail}")

            try:
                result = self._orchestrator.run(config, on_event=on_event)
                self.call_from_thread(self._append_result, scenario_key, result)
                self.call_from_thread(
                    log.write_line,
                    f"  ✓ done  hit_rate={result.prefix_hit_rate:.1%}  "
                    f"remote_frac={result.remote_fraction:.1%}  "
                    f"evictions={result.evictions}",
                )
            except Exception as exc:  # noqa: BLE001
                self.call_from_thread(log.write_line, f"  [red]✗ failed: {exc}[/red]")

        self.call_from_thread(self._set_status, f"Done: {scenario.TITLE}")
        self._running = False

    def _append_result(self, scenario_key: str, result: RunResult) -> None:
        table = self.query_one("#results", DataTable)
        scenario = _SCENARIOS[scenario_key]
        table.add_row(
            scenario.TITLE,
            result.config.routing,
            result.config.eviction,
            str(result.config.num_workers),
            f"{result.prefix_hit_rate:.1%}",
            str(result.local_hits),
            str(result.remote_hits),
            f"{result.remote_fraction:.1%}",
            str(result.evictions),
        )

    def action_clear_results(self) -> None:
        self.query_one("#results", DataTable).clear()
        self.query_one("#events", Log).clear()
        self._set_status("")

    def _set_status(self, message: str) -> None:
        self.query_one("#status-bar", Label).update(message)


def _format_event(event: dict) -> str:
    parts = []
    for key in ("tier", "hash", "source_node", "bytes_fetched", "miss_token_offset"):
        if key in event:
            parts.append(f"{key}={event[key]}")
    return "  ".join(parts)


def launch(bench_binary: Path) -> None:
    orchestrator = Orchestrator(bench_binary)
    app = KVCacheTUI(orchestrator)
    app.run()
