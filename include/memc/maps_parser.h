#pragma once

#include <memc/region.h>
#include <optional>
#include <string>
#include <vector>

namespace memc {

/// Parses /proc/<pid>/maps to extract memory region mappings.
///
/// Each line of /proc/<pid>/maps has the format:
///   address           perms offset  dev   inode   pathname
///   7f2c5c000000-7f2c5c021000 rw-p 00000000 00:00 0  [heap]
class MapsParser {
public:
  /// Parse /proc/<pid>/maps for the given PID.
  /// Returns a vector of MemoryRegion with basic fields populated.
  /// Returns std::nullopt on failure (e.g., permission denied, process gone).
  static std::optional<std::vector<MemoryRegion>> parse(pid_t pid);

  /// Parse from a raw string (useful for testing or reading from a file).
  static std::vector<MemoryRegion>
  parse_from_string(const std::string &content);

private:
  /// Parse a single line from maps into a MemoryRegion.
  static std::optional<MemoryRegion> parse_line(const std::string &line);

  /// Classify a region based on its pathname and permissions.
  static RegionType classify_region(const std::string &pathname,
                                    const std::string &permissions);
};

} // namespace memc
