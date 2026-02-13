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
struct SamplerConfig {
  pid_t pid;
  std::chrono::milliseconds interval{1000}; ///< Sampling interval
  bool use_smaps{false};                    ///< Read smaps for detailed info
  size_t max_snapshots{0}; ///< Ring buffer size (0 = unlimited)
};

/// Callback type invoked on each new snapshot.
using SnapshotCallback = std::function<void(const ProcessSnapshot &)>;

/// Periodically samples /proc/<pid>/maps (and optionally smaps)
/// and stores snapshots in a thread-safe ring buffer.
class Sampler {
public:
  explicit Sampler(SamplerConfig config);
  ~Sampler();

  // Non-copyable, non-movable (owns a thread)
  Sampler(const Sampler &) = delete;
  Sampler &operator=(const Sampler &) = delete;

  /// Start sampling in a background thread.
  void start();

  /// Stop sampling. Blocks until the background thread joins.
  void stop();

  /// Register a callback to be invoked after each snapshot is taken.
  void on_snapshot(SnapshotCallback cb);

  /// Check if the sampler is currently running.
  [[nodiscard]] bool is_running() const;

  /// Get the number of snapshots collected so far.
  [[nodiscard]] size_t snapshot_count() const;

  /// Get a copy of all collected snapshots (thread-safe).
  [[nodiscard]] std::vector<ProcessSnapshot> get_snapshots() const;

  /// Get the latest snapshot (thread-safe). Returns std::nullopt if none yet.
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
