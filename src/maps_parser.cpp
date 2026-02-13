#include <memc/maps_parser.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace memc {

std::optional<std::vector<MemoryRegion>> MapsParser::parse(pid_t pid) {
  std::string path = "/proc/" + std::to_string(pid) + "/maps";
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    return std::nullopt;
  }

  std::string content((std::istreambuf_iterator<char>(ifs)),
                      std::istreambuf_iterator<char>());
  return parse_from_string(content);
}

std::vector<MemoryRegion>
MapsParser::parse_from_string(const std::string &content) {
  std::vector<MemoryRegion> regions;
  std::istringstream stream(content);
  std::string line;

  while (std::getline(stream, line)) {
    if (line.empty())
      continue;
    auto region = parse_line(line);
    if (region) {
      regions.push_back(std::move(*region));
    }
  }

  return regions;
}

std::optional<MemoryRegion> MapsParser::parse_line(const std::string &line) {
  // Format: start-end perms offset dev inode pathname
  // Example: 7f2c5c000000-7f2c5c021000 rw-p 00000000 00:00 0  [heap]

  MemoryRegion region;

  // Parse the address range
  uint64_t start = 0, end = 0;
  char perms[8] = {};
  uint64_t offset = 0;
  char dev[16] = {};
  uint64_t inode = 0;

  // Use sscanf for the fixed-format portion
  int fields_read = std::sscanf(line.c_str(), "%lx-%lx %4s %lx %15s %lu",
                                &start, &end, perms, &offset, dev, &inode);

  if (fields_read < 6) {
    return std::nullopt;
  }

  region.start_addr = start;
  region.end_addr = end;
  region.permissions = perms;
  region.offset = offset;
  region.device = dev;
  region.inode = inode;

  // Extract the pathname (everything after the inode field)
  // Find position after the inode
  // The format is fixed-width, but the pathname column can be variable
  // We'll skip past the first 5 whitespace-delimited tokens then grab the rest
  const char *p = line.c_str();
  int spaces = 0;
  bool in_space = false;
  while (*p) {
    if (*p == ' ' || *p == '\t') {
      if (!in_space) {
        spaces++;
        in_space = true;
      }
    } else {
      in_space = false;
    }
    if (spaces >= 5 && !in_space) {
      break;
    }
    p++;
  }

  if (*p) {
    // Skip leading whitespace before pathname
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p) {
      region.pathname = p;
      // Trim trailing whitespace
      while (!region.pathname.empty() &&
             (region.pathname.back() == ' ' || region.pathname.back() == '\t' ||
              region.pathname.back() == '\n' ||
              region.pathname.back() == '\r')) {
        region.pathname.pop_back();
      }
    }
  }

  // Classify the region type
  region.type = classify_region(region.pathname, region.permissions);

  // Calculate size (without smaps, we don't have RSS, so just set size)
  region.size_kb = (end - start) / 1024;

  return region;
}

RegionType MapsParser::classify_region(const std::string &pathname,
                                       const std::string &permissions) {
  if (pathname == "[heap]") {
    return RegionType::Heap;
  }
  if (pathname.find("[stack") != std::string::npos) {
    // Matches [stack] and [stack:<tid>]
    return RegionType::Stack;
  }
  if (pathname == "[vdso]") {
    return RegionType::Vdso;
  }
  if (pathname == "[vvar]") {
    return RegionType::Vvar;
  }
  if (pathname == "[vsyscall]") {
    return RegionType::Vsyscall;
  }

  // Named file mapping
  if (!pathname.empty() && pathname[0] == '/') {
    // Check if it's a shared library
    if (pathname.find(".so") != std::string::npos) {
      return RegionType::SharedLib;
    }
    // Check if it's executable code (r-x permissions on a file)
    if (permissions.size() >= 3 && permissions[2] == 'x') {
      return RegionType::Code;
    }
    return RegionType::MappedFile;
  }

  // Anonymous mapping (no pathname, or empty after inode)
  if (pathname.empty()) {
    // Executable anonymous mapping = likely JIT code
    if (permissions.size() >= 3 && permissions[2] == 'x') {
      return RegionType::Code;
    }
    return RegionType::Anonymous;
  }

  return RegionType::Unknown;
}

} // namespace memc
