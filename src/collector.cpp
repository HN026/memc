#include <memc/collector.h>
#include <memc/maps_parser.h>
#include <memc/smaps_parser.h>

#include <chrono>

namespace memc {

DataCollector::DataCollector(pid_t pid, Config config)
    : pid_(pid), config_(std::move(config)) {}

DataCollector::~DataCollector() { stop_sampling(); }

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

std::string DataCollector::to_json(const ProcessSnapshot &snapshot) const {
  nlohmann::ordered_json j;
  memc::to_json(j, snapshot);
  if (config_.pretty_json) {
    return j.dump(2);
  }
  return j.dump();
}

void DataCollector::start_sampling() {
  if (sampler_ && sampler_->is_running())
    return;

  SamplerConfig sc;
  sc.pid = pid_;
  sc.interval = std::chrono::milliseconds(config_.interval_ms);
  sc.use_smaps = config_.use_smaps;
  sc.max_snapshots = config_.max_snapshots;

  sampler_ = std::make_unique<Sampler>(sc);

  // Rewire any pending callbacks — not applicable in fresh start
  sampler_->start();
}

void DataCollector::stop_sampling() {
  if (sampler_) {
    sampler_->stop();
  }
}

bool DataCollector::is_sampling() const {
  return sampler_ && sampler_->is_running();
}

std::vector<ProcessSnapshot> DataCollector::get_all_snapshots() const {
  if (!sampler_)
    return {};
  return sampler_->get_snapshots();
}

std::optional<ProcessSnapshot> DataCollector::get_latest_snapshot() const {
  if (!sampler_)
    return std::nullopt;
  return sampler_->get_latest();
}

void DataCollector::on_snapshot(SnapshotCallback cb) {
  if (sampler_) {
    sampler_->on_snapshot(std::move(cb));
  }
}

} // namespace memc
