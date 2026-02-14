#pragma once

#include <memc/region.h>
#include <optional>
#include <string>
#include <vector>

namespace memc {

/**
 * Parses /proc/<pid>/maps to extract memory region mappings.
 *
 * Each line of /proc/<pid>/maps has the format:
 *   address           perms offset  dev   inode   pathname
 *   7f2c5c000000-7f2c5c021000 rw-p 00000000 00:00 0  [heap]
 */
class MapsParser {
public:
  /**
   * @brief Parses /proc/<pid>/maps for the given PID.
   *
   * @param pid The process ID to parse.
   * @return std::optional<std::vector<MemoryRegion>> A vector of MemoryRegion
   * with basic fields populated, or std::nullopt on failure (e.g., permission
   * denied, process gone).
   */
  static std::optional<std::vector<MemoryRegion>> parse(pid_t pid);

  /**
   * @brief Parses memory regions from a raw string.
   *
   * This is useful for testing or reading from a file dump.
   *
   * @param content The raw content of a maps file.
   * @return std::vector<MemoryRegion> A vector of parsed MemoryRegion objects.
   */
  static std::vector<MemoryRegion>
  parse_from_string(const std::string &content);

private:
  /**
   * @brief Parses a single line from a maps file into a MemoryRegion.
   *
   * @param line The line to parse.
   * @return std::optional<MemoryRegion> The parsed MemoryRegion, or
   * std::nullopt if parsing failed.
   */
  static std::optional<MemoryRegion> parse_line(const std::string &line);

  /**
   * @brief Classifies a memory region based on its pathname and permissions.
   *
   * @param pathname The pathname associated with the region.
   * @param permissions The permissions string (e.g., "rw-p").
   * @return RegionType The classified region type.
   */
  static RegionType classify_region(const std::string &pathname,
                                    const std::string &permissions);
};

} // namespace memc
