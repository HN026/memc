#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memc/region.h>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace memc {

/// Configuration for the periodic sampler.
/**
 * @brief Configuration for the periodic Sampler.
 *
 * Fields:
 * - pid: The process ID to monitor.
 * - interval: The time duration between snapshots.
 * - use_smaps: If true, detailed memory statistics are read from smaps.
 * - max_snapshots: Size of the history ring buffer. 0 implies no limit.
 */
struct SamplerConfig {
  pid_t pid;
  std::chrono::milliseconds interval{1000};
  bool use_smaps{false};
  size_t max_snapshots{0};
};

/// Callback type invoked on each new snapshot.
using SnapshotCallback = std::function<void(const ProcessSnapshot &)>;

/**
 * Periodically samples /proc/<pid>/maps (and optionally smaps)
 * and stores snapshots in a thread-safe ring buffer.
 */
class Sampler {
public:
  explicit Sampler(SamplerConfig config);
  ~Sampler();

  // Non-copyable, non-movable (owns a thread)
  Sampler(const Sampler &) = delete;
  Sampler &operator=(const Sampler &) = delete;

  /**
   * @brief Starts the sampling thread.
   *
   * If the sampler is already running, this method does nothing.
   */
  void start();

  /**
   * @brief Stops the sampling thread.
   *
   * This method blocks until the background thread has joined.
   */
  void stop();

  /**
   * @brief Registers a callback to be invoked after each snapshot.
   *
   * @param cb The callback function.
   */
  void on_snapshot(SnapshotCallback cb);

  /**
   * @brief Checks if the sampler is currently running.
   *
   * @return true if running, false otherwise.
   */
  [[nodiscard]] bool is_running() const;

  /**
   * @brief Returns the total number of snapshots collected.
   *
   * @return size_t The number of snapshots.
   */
  [[nodiscard]] size_t snapshot_count() const;

  /**
   * @brief returns all collected snapshots.
   *
   * This operation is thread-safe and returns a copy of the internal buffer.
   *
   * @return std::vector<ProcessSnapshot> Copy of all snapshots.
   */
  [[nodiscard]] std::vector<ProcessSnapshot> get_snapshots() const;

  /**
   * @brief Returns the most recent snapshot.
   *
   * @return std::optional<ProcessSnapshot> The latest snapshot, or std::nullopt
   * if none exist.
   */
  [[nodiscard]] std::optional<ProcessSnapshot> get_latest() const;

private:
  void sample_loop();
  ProcessSnapshot take_snapshot();
  SamplerConfig config_;
  std::atomic<bool> running_{false};
  std::thread thread_;
  mutable std::mutex mutex_;
  std::vector<ProcessSnapshot> snapshots_;
  std::vector<SnapshotCallback> callbacks_;
};

} // namespace memc
