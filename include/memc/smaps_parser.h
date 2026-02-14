#pragma once

#include <memc/region.h>
#include <optional>
#include <string>
#include <vector>

namespace memc {

/**
 * Parses /proc/<pid>/smaps to enrich MemoryRegion with detailed memory info.
 *
 * smaps provides per-region details including RSS, PSS, shared/private pages,
 * swap usage, and more. Each region block starts with a header line identical
 * to /proc/<pid>/maps, followed by key-value detail lines.
 */
class SmapsParser {
public:
  /**
   * @brief Parses /proc/<pid>/smaps for the given PID.
   *
   * @param pid The process ID to parse.
   * @return std::optional<std::vector<MemoryRegion>> A vector of MemoryRegion
   * objects enriched with smaps data, or std::nullopt on failure (e.g., file
   * not found, permission denied).
   */
  static std::optional<std::vector<MemoryRegion>> parse(pid_t pid);

  /**
   * @brief Parses smaps data from a raw string.
   *
   * @param content The raw smaps content.
   * @return std::vector<MemoryRegion> A vector of parsed MemoryRegion objects.
   */
  static std::vector<MemoryRegion>
  parse_from_string(const std::string &content);

  /**
   * @brief Enriches existing MemoryRegion objects with smaps data.
   *
   * This method matches regions by start address. Regions not found in smaps
   * are left unchanged.
   *
   * @param pid The process ID to read smaps from.
   * @param regions The vector of MemoryRegion objects to enrich.
   * @return true if enrichment was successful, false otherwise.
   */
  static bool enrich(pid_t pid, std::vector<MemoryRegion> &regions);

private:
  /**
   * @brief Parses a single detail line from smaps and updates the MemoryRegion.
   *
   * @param line The detail line (e.g., "Rss: 1024 kB").
   * @param region The MemoryRegion to update.
   */
  static void apply_detail_line(const std::string &line, MemoryRegion &region);
};

} // namespace memc
