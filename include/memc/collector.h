#pragma once

#include <memc/region.h>
#include <memc/sampler.h>

#include <memory>
#include <optional>
#include <string>

namespace memc {

/**
 * @brief Configuration for the DataCollector.
 *
 * Fields:
 * - use_smaps: If true, detailed smaps data will be collected (requires more
 * overhead).
 * - interval_ms: Sampling interval in milliseconds.
 * - max_snapshots: Maximum number of snapshots to keep in history (0 =
 * unlimited).
 * - pretty_json: If true, JSON output will be indented and human-readable.
 */
struct CollectorConfig {
  bool use_smaps = false;
  uint32_t interval_ms = 1000;
  size_t max_snapshots = 0;
  bool pretty_json = true;
};

/**
 * High-level Data Collector that ties together parsing, sampling, and output.
 *
 * Usage:
 *   DataCollector collector(pid, {.use_smaps = true, .interval_ms = 500});
 *   auto snapshot = collector.collect_once();
 *   std::cout << collector.to_json(snapshot) << std::endl;
 *
 *   // Or start periodic collection:
 *   collector.start_sampling();
 *   // ... later ...
 *   collector.stop_sampling();
 *   auto all = collector.get_all_snapshots();
 */
class DataCollector {
public:
  using Config = CollectorConfig;

  /**
   * @brief Constructs a new DataCollector instance.
   *
   * @param pid The process ID to monitor.
   * @param config Configuration options for the collector. Defaults to an empty
   * configuration.
   */
  DataCollector(pid_t pid, Config config = {});
  ~DataCollector();

  /**
   * @brief Takes a single snapshot of the process memory immediately.
   *
   * @return std::optional<ProcessSnapshot> A snapshot containing memory region
   * data if successful, or std::nullopt if the process could not be accessed or
   * parsed.
   */
  [[nodiscard]] std::optional<ProcessSnapshot> collect_once();

  /**
   * @brief Serializes a process snapshot to a JSON string.
   *
   * @param snapshot The snapshot object to serialize.
   * @return std::string A JSON string representation of the snapshot.
   */
  virtual std::string to_json(const ProcessSnapshot& snapshot) const;

  /**
   * @brief Starts periodic background sampling of the process memory.
   *
   * This method initializes the internal sampler and begins collecting
   * snapshots at the configured interval. If sampling is already active, this
   * method does nothing.
   */
  void start_sampling();

  /**
   * @brief Stops the periodic background sampling.
   *
   * If sampling is not active, this method does nothing.
   */
  void stop_sampling();

  /**
   * @brief Checks if periodic sampling is currently active.
   *
   * @return true if the sampler is running, false otherwise.
   */
  [[nodiscard]] bool is_sampling() const;

  /**
   * @brief Retrieves all snapshots collected during the current sampling
   * session.
   *
   * @return std::vector<ProcessSnapshot> A list of all collected snapshots.
   */
  [[nodiscard]] std::vector<ProcessSnapshot> get_all_snapshots() const;

  /**
   * @brief Retrieves the most recently collected snapshot.
   *
   * @return std::optional<ProcessSnapshot> The latest snapshot if available, or
   * std::nullopt if no snapshots have been collected yet.
   */
  [[nodiscard]] std::optional<ProcessSnapshot> get_latest_snapshot() const;

  /**
   * @brief Registers a callback function to be invoked on each new snapshot.
   *
   * @param cb The callback function to register. It will be called with a
   * reference to the new snapshot.
   */
  void on_snapshot(SnapshotCallback cb);

  /**
   * @brief Gets the process ID being monitored.
   *
   * @return pid_t The process ID.
   */
  [[nodiscard]] pid_t pid() const { return pid_; }

private:
  pid_t pid_;
  Config config_;
  std::unique_ptr<Sampler> sampler_;
};

} // namespace memc
