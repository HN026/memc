#include <memc/maps_parser.h>
#include <memc/sampler.h>
#include <memc/smaps_parser.h>
#include <chrono>
#include <iostream>

namespace memc {

Sampler::Sampler(SamplerConfig config) : config_(std::move(config)) {}

Sampler::~Sampler() { stop(); }

void Sampler::start() {
  if (running_.load())
    return;
  running_.store(true);
  thread_ = std::thread(&Sampler::sample_loop, this);
}

void Sampler::stop() {
  running_.store(false);
  if (thread_.joinable()) {
    thread_.join();
  }
}

void Sampler::on_snapshot(SnapshotCallback cb) {
  std::lock_guard<std::mutex> lock(mutex_);
  callbacks_.push_back(std::move(cb));
}

bool Sampler::is_running() const { return running_.load(); }

size_t Sampler::snapshot_count() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshots_.size();
}

std::vector<ProcessSnapshot> Sampler::get_snapshots() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshots_;
}

std::optional<ProcessSnapshot> Sampler::get_latest() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (snapshots_.empty())
    return std::nullopt;
  return snapshots_.back();
}

void Sampler::sample_loop() {
  while (running_.load()) {
    auto snapshot = take_snapshot();

    {
      std::lock_guard<std::mutex> lock(mutex_);

      // Ring-buffer behavior: if max_snapshots is set, evict the oldest
      if (config_.max_snapshots > 0 &&
          snapshots_.size() >= config_.max_snapshots) {
        snapshots_.erase(snapshots_.begin());
      }

      snapshots_.push_back(snapshot);

      // Invoke callbacks while holding the lock
      // (callbacks should be fast; heavy processing should be async)
      for (const auto &cb : callbacks_) {
        try {
          cb(snapshot);
        } catch (const std::exception &e) {
          std::cerr << "[memc] Snapshot callback threw: " << e.what()
                    << std::endl;
        }
      }
    }

    // Sleep for the configured interval, but check running_ periodically
    // so we can stop promptly
    auto deadline = std::chrono::steady_clock::now() + config_.interval;
    while (running_.load() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
}

ProcessSnapshot Sampler::take_snapshot() {
  ProcessSnapshot snapshot;
  snapshot.pid = config_.pid;

  auto now = std::chrono::system_clock::now();
  snapshot.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now.time_since_epoch())
                              .count();

  // Parse /proc/<pid>/maps
  auto maps_result = MapsParser::parse(config_.pid);
  if (maps_result) {
    snapshot.regions = std::move(*maps_result);

    // Optionally enrich with smaps data
    if (config_.use_smaps) {
      SmapsParser::enrich(config_.pid, snapshot.regions);
    }
  }
  // If maps failed (process died?), we return an empty snapshot

  return snapshot;
}

} // namespace memc
