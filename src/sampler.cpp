#include <memc/maps_parser.h>
#include <memc/sampler.h>
#include <memc/smaps_parser.h>

#include <chrono>
#include <iostream>

namespace memc {

/**
 * @brief Constructs a Sampler with the given configuration.
 *
 * @param config The sampler configuration (PID, interval, smaps, etc.).
 */
Sampler::Sampler(SamplerConfig config) : config_(std::move(config)) {}

/**
 * @brief Destructor. Ensures the sampling thread is stopped and joined.
 */
Sampler::~Sampler() {
  stop();
}

/**
 * @brief Starts the background sampling thread.
 *
 * If the sampler is already running, this method does nothing.
 * Otherwise, it sets the running flag and spawns the sample loop thread.
 */
void Sampler::start() {
  if (running_.load()) return;
  running_.store(true);
  thread_ = std::thread(&Sampler::sample_loop, this);
}

/**
 * @brief Stops the background sampling thread.
 *
 * Sets the running flag to false and blocks until the thread has joined.
 */
void Sampler::stop() {
  running_.store(false);
  if (thread_.joinable()) {
    thread_.join();
  }
}

/**
 * @brief Registers a callback to be invoked after each snapshot.
 *
 * Thread-safe: acquires the internal mutex before modifying the callback list.
 *
 * @param cb The callback function to register.
 */
void Sampler::on_snapshot(SnapshotCallback cb) {
  std::lock_guard<std::mutex> lock(mutex_);
  callbacks_.push_back(std::move(cb));
}

/**
 * @brief Checks if the sampler is currently running.
 *
 * @return true if the sampling thread is active, false otherwise.
 */
bool Sampler::is_running() const {
  return running_.load();
}

/**
 * @brief Returns the total number of snapshots collected so far.
 *
 * Thread-safe: acquires the internal mutex.
 *
 * @return size_t The number of snapshots in the buffer.
 */
size_t Sampler::snapshot_count() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshots_.size();
}

/**
 * @brief Returns a copy of all collected snapshots.
 *
 * Thread-safe: acquires the internal mutex and returns a copy.
 *
 * @return std::vector<ProcessSnapshot> Copy of all snapshots.
 */
std::vector<ProcessSnapshot> Sampler::get_snapshots() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshots_;
}

/**
 * @brief Returns the most recent snapshot.
 *
 * Thread-safe: acquires the internal mutex.
 *
 * @return std::optional<ProcessSnapshot> The latest snapshot, or
 * std::nullopt if no snapshots have been collected.
 */
std::optional<ProcessSnapshot> Sampler::get_latest() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (snapshots_.empty()) return std::nullopt;
  return snapshots_.back();
}

/**
 * @brief The main sampling loop executed on the background thread.
 *
 * Takes a snapshot, stores it in the ring buffer (evicting the oldest
 * entry if max_snapshots is reached), invokes all registered callbacks,
 * and then sleeps until the next interval.
 */
void Sampler::sample_loop() {
  while (running_.load()) {
    auto snapshot = take_snapshot();

    {
      std::lock_guard<std::mutex> lock(mutex_);

      if (config_.max_snapshots > 0 &&
          snapshots_.size() >= config_.max_snapshots) {
        snapshots_.erase(snapshots_.begin());
      }

      snapshots_.push_back(snapshot);

      for (const auto& cb : callbacks_) {
        try {
          cb(snapshot);
        } catch (const std::exception& e) {
          std::cerr << "[memc] Snapshot callback threw: " << e.what()
                    << std::endl;
        }
      }
    }

    auto deadline = std::chrono::steady_clock::now() + config_.interval;
    while (running_.load() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
}

/**
 * @brief Takes a single process memory snapshot.
 *
 * Reads /proc/<pid>/maps and optionally enriches with smaps data.
 * The snapshot is timestamped with the current system time.
 *
 * @return ProcessSnapshot The captured snapshot.
 */
ProcessSnapshot Sampler::take_snapshot() {
  ProcessSnapshot snapshot;
  snapshot.pid = config_.pid;

  auto now = std::chrono::system_clock::now();
  snapshot.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now.time_since_epoch())
                              .count();

  if (config_.use_smaps) {
    SmapsParser::enrich(config_.pid, snapshot.regions);
  }

  return snapshot;
}

} // namespace memc
