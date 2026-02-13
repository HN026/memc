# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and
this project adheres to [Semantic Versioning](https://semver.org/).

## [1.0.0] — 2026-02-14

### Features

- **`/proc/<pid>/maps` parser** — fast, zero-copy line-by-line parser that
  extracts all memory region metadata (addresses, permissions, offset, device,
  inode, pathname).
- **`/proc/<pid>/smaps` parser** — optional enrichment with per-region RSS,
  PSS, shared/private page counts, and swap usage.
- **Region classification** — automatic categorisation of regions into `heap`,
  `stack`, `code`, `shared_lib`, `mapped_file`, `anonymous`, `vdso`, `vvar`,
  `vsyscall`, and `unknown`.
- **System-wide snapshot** (`--all`) — enumerate every process on the system,
  with per-process snapshots and a list of skipped (permission-denied)
  processes.
- **Periodic sampling** — background-threaded sampler with configurable
  interval, snapshot count, and ring-buffer history.
- **Snapshot callbacks** — register `on_snapshot` callbacks for real-time
  processing during periodic collection.
- **JSON output** — ordered JSON via [nlohmann/json](https://github.com/nlohmann/json)
  with pretty-print (default) or compact (`--compact`) modes.
- **CLI flags** — `--smaps`, `--interval`, `--count`, `--output`, `--compact`,
  `--skip-kernel`, `--help`, and `--version`.
- **Library API** — use `memc_lib` as a static library in your own CMake project;
  link with `target_link_libraries(your_target PRIVATE memc_lib)`.
- **CMake install rules** — `install(TARGETS …)` for the CLI binary, static
  library, and public headers. CMake package-config files are exported so
  downstream projects can `find_package(memc)`.

### Notes

- Requires **Linux** (`/proc` filesystem), **GCC 10+** (C++20), **CMake 3.16+**.
- First stable public release.
