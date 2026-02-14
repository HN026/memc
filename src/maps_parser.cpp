#include <cstdio>
#include <fstream>
#include <memc/maps_parser.h>
#include <sstream>
#include <string>

namespace memc {

/**
 * @brief Parses /proc/<pid>/maps for the given PID.
 *
 * Opens and reads the entire maps file, then delegates to
 * parse_from_string for line-by-line parsing.
 *
 * @param pid The process ID to parse.
 * @return std::optional<std::vector<MemoryRegion>> A vector of MemoryRegion
 * objects, or std::nullopt if the file could not be opened.
 */
std::optional<std::vector<MemoryRegion>> MapsParser::parse(pid_t pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return parse_from_string(content);
}

/**
 * @brief Parses memory regions from a raw maps-format string.
 *
 * Iterates over each line of the input, parsing it into a MemoryRegion.
 * Empty lines are skipped; malformed lines are silently ignored.
 *
 * @param content The raw content of a maps file.
 * @return std::vector<MemoryRegion> A vector of successfully parsed regions.
 */
std::vector<MemoryRegion> MapsParser::parse_from_string(const std::string& content) {
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

/**
 * @brief Parses a single line from a maps file into a MemoryRegion.
 *
 * Extracts the address range, permissions, offset, device, inode, and
 * optional pathname using sscanf, then classifies the region type.
 *
 * @param line The line to parse (e.g., "7f2c5c000000-7f2c5c021000 rw-p ...").
 * @return std::optional<MemoryRegion> The parsed region, or std::nullopt
 * if fewer than 6 fields could be read.
 */
std::optional<MemoryRegion> MapsParser::parse_line(const std::string& line) {

    MemoryRegion region;

    uint64_t start = 0, end = 0;
    char perms[8] = {};
    uint64_t offset = 0;
    char dev[16] = {};
    uint64_t inode = 0;

    int fields_read = std::sscanf(line.c_str(), "%lx-%lx %4s %lx %15s %lu", &start, &end, perms,
                                  &offset, dev, &inode);

    if (fields_read < 6) {
        return std::nullopt;
    }

    region.start_addr = start;
    region.end_addr = end;
    region.permissions = perms;
    region.offset = offset;
    region.device = dev;
    region.inode = inode;

    const char* p = line.c_str();
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
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p) {
            region.pathname = p;
            while (!region.pathname.empty() &&
                   (region.pathname.back() == ' ' || region.pathname.back() == '\t' ||
                    region.pathname.back() == '\n' || region.pathname.back() == '\r')) {
                region.pathname.pop_back();
            }
        }
    }

    region.type = classify_region(region.pathname, region.permissions);
    region.size_kb = (end - start) / 1024;

    return region;
}

/**
 * @brief Classifies a memory region based on its pathname and permissions.
 *
 * Uses pattern matching on the pathname (e.g., "[heap]", "[stack]", ".so")
 * and permission flags to determine the RegionType.
 *
 * @param pathname The pathname associated with the region.
 * @param permissions The permissions string (e.g., "rw-p").
 * @return RegionType The classified region type.
 */
RegionType MapsParser::classify_region(const std::string& pathname,
                                       const std::string& permissions) {
    if (pathname == "[heap]") {
        return RegionType::HEAP;
    }
    if (pathname.find("[stack") != std::string::npos) {
        return RegionType::STACK;
    }
    if (pathname == "[vdso]") {
        return RegionType::VDSO;
    }
    if (pathname == "[vvar]") {
        return RegionType::VVAR;
    }
    if (pathname == "[vsyscall]") {
        return RegionType::VSYSCALL;
    }

    if (!pathname.empty() && pathname[0] == '/') {
        if (pathname.find(".so") != std::string::npos) {
            return RegionType::SHARED_LIB;
        }

        if (permissions.size() >= 3 && permissions[2] == 'x') {
            return RegionType::CODE;
        }
        return RegionType::MAPPED_FILE;
    }

    if (pathname.empty()) {
        if (permissions.size() >= 3 && permissions[2] == 'x') {
            return RegionType::CODE;
        }
        return RegionType::ANONYMOUS;
    }

    return RegionType::UNKNOWN;
}

} // namespace memc
