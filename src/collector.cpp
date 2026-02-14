#include <memc/collector.h>
#include <memc/maps_parser.h>
#include <memc/smaps_parser.h>

#include <chrono>

namespace memc {

/**
 * @brief Constructs a DataCollector for the given process.
 *
 * @param pid The process ID to monitor.
 * @param config Configuration options for the collector.
 */
DataCollector::DataCollector(pid_t pid, Config config)
    : pid_(pid), config_(std::move(config)) {}

/**
 * @brief Destructor. Ensures sampling is stopped before destruction.
 */
DataCollector::~DataCollector() {
  stop_sampling();
}

/**
 * @brief Takes a single snapshot of the process memory.
 *
 * Reads /proc/<pid>/maps and optionally enriches with smaps data.
 * The snapshot is timestamped with the current system time.
 *
 * @return std::optional<ProcessSnapshot> A snapshot on success, or
 * std::nullopt if the process could not be accessed.
 */
std::optional<ProcessSnapshot> DataCollector::collect_once() {
  ProcessSnapshot snapshot;
  snapshot.pid = pid_;

  auto now = std::chrono::system_clock::now();
  snapshot.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now.time_since_epoch())
                              .count();

  auto maps_result = MapsParser::parse(pid_);
  if (!maps_result) {
    return std::nullopt;
  }

  snapshot.regions = std::move(*maps_result);

  if (config_.use_smaps) {
    SmapsParser::enrich(pid_, snapshot.regions);
  }

  return snapshot;
}

/**
 * @brief Serializes a snapshot to a JSON string.
 *
 * Uses ordered JSON to preserve key insertion order. Output format
 * (pretty vs compact) is controlled by the collector's configuration.
 *
 * @param snapshot The snapshot to serialize.
 * @return std::string The JSON string representation.
 */
std::string DataCollector::to_json(const ProcessSnapshot& snapshot) const {
  nlohmann::ordered_json j;
  memc::to_json(j, snapshot);
  if (config_.pretty_json) {
    return j.dump(2);
  }
  return j.dump();
}

/**
 * @brief Starts periodic background sampling.
 *
 * Creates a Sampler with the collector's configuration and begins
 * collecting snapshots at the configured interval. Does nothing if
 * sampling is already active.
 */
void DataCollector::start_sampling() {
  if (sampler_ && sampler_->is_running()) return;

  SamplerConfig sc;
  sc.pid = pid_;
  sc.interval = std::chrono::milliseconds(config_.interval_ms);
  sc.use_smaps = config_.use_smaps;
  sc.max_snapshots = config_.max_snapshots;

  sampler_ = std::make_unique<Sampler>(sc);

  sampler_->start();
}

/**
 * @brief Stops periodic background sampling.
 *
 * If no sampler is active, this method does nothing.
 */
void DataCollector::stop_sampling() {
  if (sampler_) {
    sampler_->stop();
  }
}

/**
 * @brief Checks if periodic sampling is currently active.
 *
 * @return true if the sampler exists and is running, false otherwise.
 */
bool DataCollector::is_sampling() const {
  return sampler_ && sampler_->is_running();
}

/**
 * @brief Retrieves all snapshots collected during the current sampling session.
 *
 * @return std::vector<ProcessSnapshot> A list of all collected snapshots,
 * or an empty vector if no sampler is active.
 */
std::vector<ProcessSnapshot> DataCollector::get_all_snapshots() const {
  if (!sampler_) return {};
  return sampler_->get_snapshots();
}

/**
 * @brief Retrieves the most recently collected snapshot.
 *
 * @return std::optional<ProcessSnapshot> The latest snapshot, or
 * std::nullopt if no sampler is active or no snapshots exist.
 */
std::optional<ProcessSnapshot> DataCollector::get_latest_snapshot() const {
  if (!sampler_) return std::nullopt;
  return sampler_->get_latest();
}

/**
 * @brief Registers a callback to be invoked on each new snapshot.
 *
 * The callback is forwarded to the internal sampler. If no sampler
 * is active, the callback is silently dropped.
 *
 * @param cb The callback function to register.
 */
void DataCollector::on_snapshot(SnapshotCallback cb) {
  if (sampler_) {
    sampler_->on_snapshot(std::move(cb));
  }
}

} // namespace memc
