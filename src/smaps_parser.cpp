
#include <cstdio>
#include <fstream>
#include <memc/maps_parser.h>
#include <memc/smaps_parser.h>
#include <sstream>
#include <string>
#include <unordered_map>

namespace memc {

/**
 * @brief Parses /proc/<pid>/smaps for the given PID.
 *
 * Opens and reads the entire smaps file, then delegates to
 * parse_from_string for structured parsing.
 *
 * @param pid The process ID to parse.
 * @return std::optional<std::vector<MemoryRegion>> A vector of enriched
 * MemoryRegion objects, or std::nullopt if the file could not be opened.
 */
std::optional<std::vector<MemoryRegion>> SmapsParser::parse(pid_t pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/smaps";
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return parse_from_string(content);
}

/**
 * @brief Parses smaps data from a raw string.
 *
 * Iterates over lines: header lines (starting with a hex digit) begin
 * a new region, and subsequent detail lines update that region's fields.
 *
 * @param content The raw smaps content.
 * @return std::vector<MemoryRegion> A vector of parsed MemoryRegion objects
 * with smaps detail fields populated.
 */
std::vector<MemoryRegion> SmapsParser::parse_from_string(const std::string& content) {
    std::vector<MemoryRegion> regions;
    std::istringstream stream(content);
    std::string line;

    std::optional<size_t> current_idx;

    while (std::getline(stream, line)) {
        if (line.empty())
            continue;

        if (!line.empty() && std::isxdigit(line[0])) {
            auto parsed = MapsParser::parse_from_string(line + "\n");
            if (!parsed.empty()) {
                regions.push_back(std::move(parsed[0]));
                current_idx = regions.size() - 1;
                regions[*current_idx].has_smaps_data = true;
            }
        } else if (current_idx) {
            apply_detail_line(line, regions[*current_idx]);
        }
    }

    return regions;
}

/**
 * @brief Enriches existing MemoryRegion objects with smaps data.
 *
 * Parses /proc/<pid>/smaps, builds a lookup table by start address,
 * and copies smaps fields (RSS, PSS, swap, etc.) into matching regions.
 * Regions not found in smaps are left unchanged.
 *
 * @param pid The process ID to read smaps from.
 * @param regions The vector of MemoryRegion objects to enrich.
 * @return true if smaps was successfully parsed, false otherwise.
 */
bool SmapsParser::enrich(pid_t pid, std::vector<MemoryRegion>& regions) {
    auto smaps_result = parse(pid);
    if (!smaps_result) {
        return false;
    }

    std::unordered_map<uint64_t, size_t> smaps_lookup;
    for (size_t i = 0; i < smaps_result->size(); ++i) {
        smaps_lookup[(*smaps_result)[i].start_addr] = i;
    }

    for (auto& region : regions) {
        auto it = smaps_lookup.find(region.start_addr);
        if (it != smaps_lookup.end()) {
            const auto& sr = (*smaps_result)[it->second];
            region.rss_kb = sr.rss_kb;
            region.pss_kb = sr.pss_kb;
            region.shared_clean_kb = sr.shared_clean_kb;
            region.shared_dirty_kb = sr.shared_dirty_kb;
            region.private_clean_kb = sr.private_clean_kb;
            region.private_dirty_kb = sr.private_dirty_kb;
            region.swap_kb = sr.swap_kb;
            region.has_smaps_data = true;
        }
    }

    return true;
}

/**
 * @brief Parses a single detail line from smaps and updates the MemoryRegion.
 *
 * Splits the line on ':', extracts the numeric value, and updates the
 * corresponding field (Size, Rss, Pss, Shared_Clean, etc.) on the region.
 *
 * @param line The detail line (e.g., "Rss:           1024 kB").
 * @param region The MemoryRegion to update.
 */
void SmapsParser::apply_detail_line(const std::string& line, MemoryRegion& region) {
    auto colon_pos = line.find(':');
    if (colon_pos == std::string::npos)
        return;

    std::string key = line.substr(0, colon_pos);
    std::string value_part = line.substr(colon_pos + 1);

    uint64_t value = 0;
    std::sscanf(value_part.c_str(), " %lu", &value);

    if (key == "Size") {
        region.size_kb = value;
    } else if (key == "Rss") {
        region.rss_kb = value;
    } else if (key == "Pss") {
        region.pss_kb = value;
    } else if (key == "Shared_Clean") {
        region.shared_clean_kb = value;
    } else if (key == "Shared_Dirty") {
        region.shared_dirty_kb = value;
    } else if (key == "Private_Clean") {
        region.private_clean_kb = value;
    } else if (key == "Private_Dirty") {
        region.private_dirty_kb = value;
    } else if (key == "Swap") {
        region.swap_kb = value;
    }
}

} // namespace memc
