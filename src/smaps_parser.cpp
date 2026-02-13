#include <memc/maps_parser.h>
#include <memc/smaps_parser.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace memc {

std::optional<std::vector<MemoryRegion>> SmapsParser::parse(pid_t pid) {
  std::string path = "/proc/" + std::to_string(pid) + "/smaps";
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    return std::nullopt;
  }

  std::string content((std::istreambuf_iterator<char>(ifs)),
                      std::istreambuf_iterator<char>());
  return parse_from_string(content);
}

std::vector<MemoryRegion>
SmapsParser::parse_from_string(const std::string &content) {
  std::vector<MemoryRegion> regions;
  std::istringstream stream(content);
  std::string line;

  MemoryRegion *current_region = nullptr;

  while (std::getline(stream, line)) {
    if (line.empty())
      continue;

    // Header lines look like maps lines: start with hex address
    // Detail lines look like: Key:  value kB
    if (!line.empty() && std::isxdigit(line[0])) {
      // This is a header line — parse it like a maps entry
      auto parsed = MapsParser::parse_from_string(line + "\n");
      if (!parsed.empty()) {
        regions.push_back(std::move(parsed[0]));
        current_region = &regions.back();
        current_region->has_smaps_data = true;
      }
    } else if (current_region) {
      // This is a detail line for the current region
      apply_detail_line(line, *current_region);
    }
  }

  return regions;
}

bool SmapsParser::enrich(pid_t pid, std::vector<MemoryRegion> &regions) {
  auto smaps_result = parse(pid);
  if (!smaps_result) {
    return false;
  }

  // Build a lookup table by start address for the smaps data
  std::unordered_map<uint64_t, const MemoryRegion *> smaps_lookup;
  for (const auto &sr : *smaps_result) {
    smaps_lookup[sr.start_addr] = &sr;
  }

  // Enrich each region
  for (auto &region : regions) {
    auto it = smaps_lookup.find(region.start_addr);
    if (it != smaps_lookup.end()) {
      const auto &sr = *it->second;
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

void SmapsParser::apply_detail_line(const std::string &line,
                                    MemoryRegion &region) {
  // Detail lines have the format: "Key:         value kB"
  // We'll extract the key and value

  auto colon_pos = line.find(':');
  if (colon_pos == std::string::npos)
    return;

  std::string key = line.substr(0, colon_pos);
  std::string value_part = line.substr(colon_pos + 1);

  // Trim whitespace and extract numeric value
  uint64_t value = 0;
  std::sscanf(value_part.c_str(), " %lu", &value);

  // Apply known keys
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
  // We silently ignore other keys (Referenced, Anonymous, LazyFree, etc.)
  // They can be added later as needed.
}

} // namespace memc
