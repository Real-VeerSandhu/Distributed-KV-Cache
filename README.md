# distributed-kv-cache

A distributed, block-based KV cache simulator for LLM prefix reuse.

It will model prefix lookup, block-level reuse, tiered memory, eviction, distributed discovery,
remote fetch, routing policies, deterministic benchmarking, and observability.

---

## Current status

**Development environment only.**

The source architecture has not been designed yet. This repo contains only the build toolchain,
dependency setup, linting/formatting configuration, and a small environment smoke test.

---

## Prerequisites

| Tool | Minimum version |
| ---- | --------------- |
| CMake | 3.20 |
| C++ compiler (GCC or Clang) | C++17 support |
| Git | any recent |
| clang-format | 14+ (for formatting) |
| clang-tidy | 14+ (for linting) |

Dependencies (GoogleTest, nlohmann/json) are fetched automatically by CMake via FetchContent.

---

## Build

```bash
./scripts/build.sh
```

This configures and builds the project without tests. The build directory is `build/`.

To control the build type:

```bash
BUILD_TYPE=Debug ./scripts/build.sh
```

---

## Test

```bash
./scripts/test.sh
```

This configures a separate `build-test/` directory with `BUILD_TESTING=ON`, builds, and runs
all tests via CTest.

---

## Formatting

```bash
./scripts/format.sh
```

Runs `clang-format` over the `tests/` directory. Extend the `DIRS` array in the script when
`src/` and `apps/` are added.

---

## What is intentionally not included yet

- `src/` — no project source files
- `apps/` — no executable targets (worker, coordinator, benchmark)
- `configs/` — no runtime configuration files
- No cache classes, policy classes, block/prefix types, transport layer, or any domain logic
- No Clock, Config, Policy, Block, Prefix, or Transport skeletons

---

## What comes next

After the design doc and class design doc are finalized:

1. Add `src/` with the core library (cache, block manager, eviction policy, prefix index)
2. Add `apps/` with worker, coordinator, and benchmark binaries
3. Add `configs/` with example JSON configuration files
4. Expand `tests/` with unit and integration tests for each component
5. Wire up the full CMake target graph
6. Add CI configuration
