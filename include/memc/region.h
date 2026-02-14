#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <third_party/nlohmann/json.hpp>
#include <vector>

namespace memc {

/**
 * Classification of memory region types derived from the mapping path and
 * flags.
 */
enum class RegionType {
  HEAP,
  STACK,
  CODE,
  SHARED_LIB,
  VDSO,
  VVAR,
  VSYSCALL,
  MAPPED_FILE,
  ANONYMOUS,
  UNKNOWN
};

/**
 * @brief Converts a RegionType enum value to its string representation.
 *
 * @param t The RegionType value to convert.
 * @return const char* A string literal representing the region type (e.g.,
 * "heap", "stack"). Returns "unknown" if the type is not recognized.
 */
inline const char *region_type_to_string(RegionType t) {
  switch (t) {
  case RegionType::HEAP:
    return "heap";
  case RegionType::STACK:
    return "stack";
  case RegionType::CODE:
    return "code";
  case RegionType::SHARED_LIB:
    return "shared_lib";
  case RegionType::VDSO:
    return "vdso";
  case RegionType::VVAR:
    return "vvar";
  case RegionType::VSYSCALL:
    return "vsyscall";
  case RegionType::MAPPED_FILE:
    return "mapped_file";
  case RegionType::ANONYMOUS:
    return "anonymous";
  case RegionType::UNKNOWN:
    return "unknown";
  }
  return "unknown";
}

/**
 * @brief A single memory region parsed from /proc/<pid>/maps (and optionally
 * smaps).
 *
 * This struct holds the details of a virtual memory area (VMA).
 *
 * Fields:
 * - start_addr: Start address of the mapping.
 * - end_addr: End address of the mapping.
 * - permissions: Permission string (e.g., "rw-p").
 * - offset: File offset.
 * - device: Device ID (major:minor).
 * - inode: Inode number.
 * - pathname: Mapped file path or label (e.g., "[heap]").
 * - type: Calssified region type.
 *
 * Extended Smaps Fields:
 * - size_kb: Size of the mapping in KB.
 * - rss_kb: Resident Set Size in KB.
 * - pss_kb: Proportional Set Size in KB.
 * - shared_clean_kb: Shared clean pages in KB.
 * - shared_dirty_kb: Shared dirty pages in KB.
 * - private_clean_kb: Private clean pages in KB.
 * - private_dirty_kb: Private dirty pages in KB.
 * - swap_kb: Swap usage in KB.
 * - has_smaps_data: True if smaps fields are populated.
 */
struct MemoryRegion {
  uint64_t start_addr = 0;
  uint64_t end_addr = 0;
  std::string permissions;
  uint64_t offset = 0;
  std::string device;
  uint64_t inode = 0;
  std::string pathname;

  RegionType type = RegionType::UNKNOWN;

  uint64_t size_kb = 0;
  uint64_t rss_kb = 0;
  uint64_t pss_kb = 0;
  uint64_t shared_clean_kb = 0;
  uint64_t shared_dirty_kb = 0;
  uint64_t private_clean_kb = 0;
  uint64_t private_dirty_kb = 0;
  uint64_t swap_kb = 0;

  bool has_smaps_data = false;

  /**
   * @brief Calculates the total size of this memory region in bytes.
   *
   * @return uint64_t The size of the region (end_addr - start_addr).
   */
  [[nodiscard]] uint64_t size_bytes() const { return end_addr - start_addr; }
};

/**
 * @brief Serializes a MemoryRegion object to an ordered JSON object.
 *
 * This function converts the MemoryRegion's fields into a JSON representation,
 * formatting addresses as hex strings into temporary buffers and preserving
 * the insertion order of keys.
 *
 * @param j The JSON object to populate.
 * @param r The MemoryRegion object to serialize.
 */
inline void to_json(nlohmann::ordered_json &j, const MemoryRegion &r) {
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

/**
 * @brief A snapshot of all memory regions for a process at a point in time.
 *
 * Fields:
 * - pid: Process ID.
 * - timestamp_ms: UNIX epoch milliseconds.
 * - regions: List of memory regions.
 */
struct ProcessSnapshot {
  pid_t pid;
  uint64_t timestamp_ms;
  std::vector<MemoryRegion> regions;

  /**
   * @brief Calculates the total Resident Set Size (RSS) across all memory
   * regions.
   *
   * This value is only meaningful if smaps data is present in the regions.
   *
   * @return uint64_t The total RSS in kilobytes.
   */
  [[nodiscard]] uint64_t total_rss_kb() const {
    uint64_t total = 0;
    for (const auto &r : regions)
      total += r.rss_kb;
    return total;
  }

  /**
   * @brief Calculates the total virtual memory size across all memory regions.
   *
   * @return uint64_t The total virtual memory size in kilobytes.
   */
  [[nodiscard]] uint64_t total_vsize_kb() const {
    uint64_t total = 0;
    for (const auto &r : regions)
      total += r.size_bytes();
    return total / 1024;
  }
};

/**
 * @brief Serializes a ProcessSnapshot object to an ordered JSON object.
 *
 * This function converts the ProcessSnapshot and its list of MemoryRegions
 * into a JSON representation. The list of regions is manually built as an
 * array of ordered JSON objects to maintain order.
 *
 * @param j The JSON object to populate.
 * @param s The ProcessSnapshot object to serialize.
 */
inline void to_json(nlohmann::ordered_json &j, const ProcessSnapshot &s) {
  j = nlohmann::ordered_json{};
  j["pid"] = s.pid;
  j["timestamp_ms"] = s.timestamp_ms;
  j["total_rss_kb"] = s.total_rss_kb();
  j["total_vsize_kb"] = s.total_vsize_kb();
  j["region_count"] = s.regions.size();

  j["regions"] = nlohmann::ordered_json::array();
  for (const auto &r : s.regions) {
    nlohmann::ordered_json rj;
    to_json(rj, r);
    j["regions"].push_back(std::move(rj));
  }
}

} // namespace memc
