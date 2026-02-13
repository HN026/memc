#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <third_party/nlohmann/json.hpp>
#include <vector>

namespace memc {

/// Classification of memory region types derived from the mapping path and
/// flags.
enum class RegionType {
  Heap,
  Stack,
  Code,      // executable text segments
  SharedLib, // .so mappings
  Vdso,
  Vvar,
  Vsyscall,
  MappedFile,
  Anonymous,
  Unknown
};

/// Convert RegionType to a human-readable string.
inline const char *region_type_to_string(RegionType t) {
  switch (t) {
  case RegionType::Heap:
    return "heap";
  case RegionType::Stack:
    return "stack";
  case RegionType::Code:
    return "code";
  case RegionType::SharedLib:
    return "shared_lib";
  case RegionType::Vdso:
    return "vdso";
  case RegionType::Vvar:
    return "vvar";
  case RegionType::Vsyscall:
    return "vsyscall";
  case RegionType::MappedFile:
    return "mapped_file";
  case RegionType::Anonymous:
    return "anonymous";
  case RegionType::Unknown:
    return "unknown";
  }
  return "unknown";
}

/// A single memory region parsed from /proc/<pid>/maps (and optionally smaps).
struct MemoryRegion {
  uint64_t start_addr = 0; ///< Start address of the mapping
  uint64_t end_addr = 0;   ///< End address of the mapping
  std::string permissions; ///< e.g. "rw-p", "r-xp"
  uint64_t offset = 0;     ///< File offset
  std::string device;      ///< Device (major:minor)
  uint64_t inode = 0;      ///< Inode number
  std::string pathname;    ///< Mapped file path or label (e.g. "[heap]")

  RegionType type = RegionType::Unknown;

  // --- Extended fields from smaps (optional, 0 if not populated) ---
  uint64_t size_kb = 0; ///< Size of the mapping in KB
  uint64_t rss_kb = 0;  ///< Resident Set Size in KB
  uint64_t pss_kb = 0;  ///< Proportional Set Size in KB
  uint64_t shared_clean_kb = 0;
  uint64_t shared_dirty_kb = 0;
  uint64_t private_clean_kb = 0;
  uint64_t private_dirty_kb = 0;
  uint64_t swap_kb = 0; ///< Swap usage in KB

  bool has_smaps_data = false; ///< Whether smaps detail has been populated

  /// Calculate the total size of this region in bytes from addresses.
  [[nodiscard]] uint64_t size_bytes() const { return end_addr - start_addr; }
};

/// Serialize a MemoryRegion to ordered JSON (preserves key insertion order).
inline void to_json(nlohmann::ordered_json &j, const MemoryRegion &r) {
  // Format addresses as hex strings
  char start_buf[32], end_buf[32];
  std::snprintf(start_buf, sizeof(start_buf), "0x%lx", r.start_addr);
  std::snprintf(end_buf, sizeof(end_buf), "0x%lx", r.end_addr);

  j = nlohmann::ordered_json{};
  j["start"] = start_buf;
  j["end"] = end_buf;
  j["type"] = region_type_to_string(r.type);
  j["perm"] = r.permissions;
  j["size_kb"] = r.size_bytes() / 1024;

  if (!r.pathname.empty()) {
    j["pathname"] = r.pathname;
  }

  if (r.has_smaps_data) {
    j["rss_kb"] = r.rss_kb;
    j["pss_kb"] = r.pss_kb;
    j["shared_clean_kb"] = r.shared_clean_kb;
    j["shared_dirty_kb"] = r.shared_dirty_kb;
    j["private_clean_kb"] = r.private_clean_kb;
    j["private_dirty_kb"] = r.private_dirty_kb;
    j["swap_kb"] = r.swap_kb;
  }
}

/// A snapshot of all memory regions for a process at a point in time.
struct ProcessSnapshot {
  pid_t pid;
  uint64_t timestamp_ms; ///< UNIX epoch milliseconds
  std::vector<MemoryRegion> regions;

  /// Total RSS across all regions (only meaningful if smaps data is present).
  [[nodiscard]] uint64_t total_rss_kb() const {
    uint64_t total = 0;
    for (const auto &r : regions)
      total += r.rss_kb;
    return total;
  }

  /// Total virtual memory size in KB.
  [[nodiscard]] uint64_t total_vsize_kb() const {
    uint64_t total = 0;
    for (const auto &r : regions)
      total += r.size_bytes();
    return total / 1024;
  }
};

/// Serialize a ProcessSnapshot to ordered JSON.
inline void to_json(nlohmann::ordered_json &j, const ProcessSnapshot &s) {
  j = nlohmann::ordered_json{};
  j["pid"] = s.pid;
  j["timestamp_ms"] = s.timestamp_ms;
  j["total_rss_kb"] = s.total_rss_kb();
  j["total_vsize_kb"] = s.total_vsize_kb();
  j["region_count"] = s.regions.size();

  // Manually build ordered regions array
  j["regions"] = nlohmann::ordered_json::array();
  for (const auto &r : s.regions) {
    nlohmann::ordered_json rj;
    to_json(rj, r);
    j["regions"].push_back(std::move(rj));
  }
}

} // namespace memc
