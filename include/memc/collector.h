#pragma once

#include <memc/region.h>
#include <memc/sampler.h>
#include <memory>
#include <optional>
#include <string>

namespace memc {

/// Configuration for the DataCollector.
struct CollectorConfig {
  bool use_smaps = false;      ///< Enrich with smaps detail
  uint32_t interval_ms = 1000; ///< Sampling interval in ms
  size_t max_snapshots = 0;    ///< 0 = unlimited history
  bool pretty_json = true;     ///< Pretty-print JSON output
};

/// High-level Data Collector that ties together parsing, sampling, and output.
///
/// Usage:
///   DataCollector collector(pid, {.use_smaps = true, .interval_ms = 500});
///   auto snapshot = collector.collect_once();
///   std::cout << collector.to_json(snapshot) << std::endl;
///
///   // Or start periodic collection:
///   collector.start_sampling();
///   // ... later ...
///   collector.stop_sampling();
///   auto all = collector.get_all_snapshots();
class DataCollector {
public:
  using Config = CollectorConfig;

  DataCollector(pid_t pid, Config config = {});
  ~DataCollector();

  /// Take a single snapshot right now and return it.
  [[nodiscard]] std::optional<ProcessSnapshot> collect_once();

  /// Serialize a snapshot to a JSON string.
  [[nodiscard]] std::string to_json(const ProcessSnapshot &snapshot) const;

  /// Start periodic background sampling.
  void start_sampling();

  /// Stop periodic sampling.
  void stop_sampling();

  /// Check if periodic sampling is active.
  [[nodiscard]] bool is_sampling() const;

  /// Get all snapshots from periodic sampling.
  [[nodiscard]] std::vector<ProcessSnapshot> get_all_snapshots() const;

  /// Get the latest snapshot from periodic sampling.
  [[nodiscard]] std::optional<ProcessSnapshot> get_latest_snapshot() const;

  /// Register a callback for each new snapshot during periodic sampling.
  void on_snapshot(SnapshotCallback cb);

  /// Get the PID being monitored.
  [[nodiscard]] pid_t pid() const { return pid_; }

private:
  pid_t pid_;
  Config config_;
  std::unique_ptr<Sampler> sampler_;
};

} // namespace memc
