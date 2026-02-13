#pragma once

#include <memc/region.h>
#include <optional>
#include <string>
#include <vector>

namespace memc {

/// Parses /proc/<pid>/smaps to enrich MemoryRegion with detailed memory info.
///
/// smaps provides per-region details including RSS, PSS, shared/private pages,
/// swap usage, and more. Each region block starts with a header line identical
/// to /proc/<pid>/maps, followed by key-value detail lines.
class SmapsParser {
public:
  /// Parse /proc/<pid>/smaps for the given PID.
  /// Returns fully-populated MemoryRegion entries with smaps detail.
  /// Returns std::nullopt on failure (permission denied, kernel config, etc.).
  static std::optional<std::vector<MemoryRegion>> parse(pid_t pid);

  /// Parse from a raw string (useful for testing).
  static std::vector<MemoryRegion>
  parse_from_string(const std::string &content);

  /// Enrich an existing vector of MemoryRegions (from maps) with smaps data.
  /// Matches regions by start address. Regions not found in smaps are
  /// unchanged.
  static bool enrich(pid_t pid, std::vector<MemoryRegion> &regions);

private:
  /// Parse a detail line like "Rss:       132 kB" and apply it to the region.
  static void apply_detail_line(const std::string &line, MemoryRegion &region);
};

} // namespace memc
