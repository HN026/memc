# memc — Linux Process Memory Collector

> **Version 1.0.0** · [Changelog](CHANGELOG.md) · [License: MIT](LICENSE)

A lightweight C++ library that reads `/proc/<pid>/maps` (and optionally `/proc/<pid>/smaps`) and outputs structured JSON snapshots of a process's memory regions.

---

## Prerequisites

- **Linux** (uses `/proc` filesystem)
- **GCC 10+** or any compiler with C++20 support
- **CMake 3.16+**

## Build

```bash
# Clone / navigate to the project
cd memc

# Configure and build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The binary is produced at `./build/memc`.

## Install (Optional)

```bash
# Install to /usr/local (headers, static lib, CLI binary, and CMake package config)
sudo cmake --install build

# Or install to a custom prefix
cmake --install build --prefix /opt/memc
```

After installing, downstream CMake projects can use:

```cmake
find_package(memc 1.0 REQUIRED)
target_link_libraries(your_target PRIVATE memc::memc_lib)
```

## Usage

```
memc <pid> [options]
memc --all [options]
```

### Options

| Flag              | Description                                       | Default |
| ----------------- | ------------------------------------------------- | ------- |
| `--all`           | Snapshot ALL processes on the system              | off     |
| `--smaps`         | Enable detailed smaps data (RSS, PSS, swap, etc.) | off     |
| `--output <file>` | Write JSON to a file instead of stdout            | stdout  |
| `--interval <ms>` | Sampling interval in milliseconds                 | 1000    |
| `--count <n>`     | Number of samples (0 = continuous until Ctrl+C)   | 1       |
| `--compact`       | Output compact JSON instead of pretty-printed     | off     |
| `--skip-kernel`   | Skip kernel threads with no user-space memory     | off     |
| `--version`       | Show version information                          | —       |
| `--help`          | Show help message                                 | —       |

### Examples

```bash
# Single snapshot of a process
./build/memc 1234

# Snapshot with detailed memory info (RSS, PSS, swap, shared/private pages)
./build/memc 1234 --smaps

# Monitor the current shell
./build/memc $$

# ── System-wide snapshots ──────────────────────────────
# Snapshot ALL processes with smaps detail
./build/memc --all --smaps

# Save system-wide snapshot to a file
./build/memc --all --smaps --output system.json

# Compact system-wide snapshot (smaller file)
./build/memc --all --smaps --compact --output system.json

# ── Periodic sampling ─────────────────────────────────
# Continuous sampling every 500ms (Ctrl+C to stop)
./build/memc 1234 --count 0 --interval 500

# Take 5 samples, 2 seconds apart, compact JSON
./build/memc 1234 --count 5 --interval 2000 --compact

# Pipe to jq for quick filtering
./build/memc $$ --smaps | jq '.regions[] | select(.type == "heap")'
```

## Output Formatd

All output goes to **stdout** (status messages go to stderr), or to a file with `--output`.

### Single Process (`memc <pid>`)

```json
{
  "pid": 1234,
  "timestamp_ms": 1771011124506,
  "total_rss_kb": 4820,
  "total_vsize_kb": 233592,
  "region_count": 29,
  "regions": [
    {
      "start": "0x5583794a9000",
      "end": "0x5583795ac000",
      "type": "code",
      "perm": "r-xp",
      "size_kb": 1036,
      "pathname": "/usr/bin/bash"
    },
    {
      "start": "0x5583a7c32000",
      "end": "0x5583a7dda000",
      "type": "heap",
      "perm": "rw-p",
      "size_kb": 1696,
      "pathname": "[heap]",
      "rss_kb": 132,
      "pss_kb": 132,
      "shared_clean_kb": 0,
      "shared_dirty_kb": 0,
      "private_clean_kb": 0,
      "private_dirty_kb": 132,
      "swap_kb": 0
    }
  ]
}
```

### System-wide (`memc --all`)

```json
{
  "timestamp_ms": 1771011727828,
  "process_count": 457,
  "skipped": 60,
  "processes": [
    {
      "pid": 1234,
      "name": "bash",
      "snapshot": {
        "pid": 1234,
        "region_count": 29,
        "regions": [ ... ],
        "total_rss_kb": 4820,
        "total_vsize_kb": 233592
      }
    }
  ]
}
```

> **Note:** The `rss_kb`, `pss_kb`, `shared_*`, `private_*`, and `swap_kb` fields only appear when `--smaps` is enabled. The `skipped` field in `--all` mode shows processes that couldn't be read (usually due to permissions).

### Region Types

| Type          | Description                          |
| ------------- | ------------------------------------ |
| `heap`        | Process heap (`[heap]`)              |
| `stack`       | Thread/process stack (`[stack]`)     |
| `code`        | Executable text segments             |
| `shared_lib`  | Shared library mappings (`.so`)      |
| `mapped_file` | Memory-mapped files                  |
| `anonymous`   | Anonymous mappings (no backing file) |
| `vdso`        | Virtual Dynamic Shared Object        |
| `vvar`        | Kernel vvar page                     |
| `vsyscall`    | Legacy vsyscall page                 |
| `unknown`     | Unclassified mapping                 |

## Project Structure

```
memc/
├── CMakeLists.txt                  # Build configuration (C++20, static lib + CLI)
├── CHANGELOG.md                    # Release history
├── LICENSE                         # MIT license
├── README.md
├── cmake/
│   └── memcConfig.cmake.in         # CMake package config template
├── main.cpp                        # CLI entry point
├── include/memc/
│   ├── version.h                  # Version macros (MEMC_VERSION_*)
│   ├── region.h                   # MemoryRegion & ProcessSnapshot data types + JSON
│   ├── maps_parser.h              # /proc/<pid>/maps parser
│   ├── smaps_parser.h             # /proc/<pid>/smaps parser (optional detail)
│   ├── sampler.h                  # Periodic background sampler (threaded)
│   └── collector.h                # DataCollector high-level facade
├── src/
│   ├── maps_parser.cpp
│   ├── smaps_parser.cpp
│   ├── sampler.cpp
│   └── collector.cpp
└── third_party/
    └── nlohmann/json.hpp          # Header-only JSON library (v3.11.3)
```

## Library API

You can also use `memc` as a library in your own C++ project:

```cpp
#include <memc/collector.h>
#include <memc/version.h>
#include <iostream>

int main() {
    std::cout << "memc v" << MEMC_VERSION_STRING << "\n";

    pid_t target_pid = 1234;

    // Single snapshot
    memc::DataCollector collector(target_pid, {.use_smaps = true});
    auto snapshot = collector.collect_once();
    if (snapshot) {
        std::cout << collector.to_json(*snapshot) << std::endl;
    }

    // Periodic sampling with callback
    memc::DataCollector sampler(target_pid, {
        .use_smaps    = true,
        .interval_ms  = 500,
        .max_snapshots = 100   // ring buffer of last 100
    });
    sampler.on_snapshot([](const memc::ProcessSnapshot& snap) {
        std::cout << "RSS: " << snap.total_rss_kb() << " KB\n";
    });
    sampler.start_sampling();
    // ... do work ...
    sampler.stop_sampling();
}
```

Link against `memc_lib` and `pthread` in your CMake:

```cmake
target_link_libraries(your_target PRIVATE memc_lib)
```

Or, if you installed memc system-wide:

```cmake
find_package(memc 1.0 REQUIRED)
target_link_libraries(your_target PRIVATE memc::memc_lib)
```

## Permissions & Notes

### Skipped processes (`skipped_processes` in `--all` mode)

When running `./build/memc --all`, some processes will appear in the `skipped_processes` array. These are typically **root-owned system services** (systemd, dockerd, NetworkManager, etc.) whose `/proc/<pid>/maps` cannot be read by a regular user.

To collect **all** processes including these, run with elevated privileges:

```bash
sudo ./build/memc --all --smaps --output full_system.json
```

### Kernel threads with empty regions

You'll notice many processes (like `kworker`, `ksoftirqd`, `migration`, `rcu_preempt`, etc.) show `region_count: 0` with an empty `regions` array. This is **expected** — these are **kernel threads** that run entirely in kernel space and have **no user-space virtual memory mappings**. Their `/proc/<pid>/maps` is legitimately empty.

### Permission summary

| Access                                       | Requirement                   |
| -------------------------------------------- | ----------------------------- |
| `/proc/<pid>/maps` (own processes)           | No special permissions needed |
| `/proc/<pid>/maps` (other users' processes)  | Root or `CAP_SYS_PTRACE`      |
| `/proc/<pid>/smaps` (own processes)          | No special permissions needed |
| `/proc/<pid>/smaps` (other users' processes) | Root or `CAP_SYS_PTRACE`      |

## License

[MIT](LICENSE) © 2026 Huzaifa
