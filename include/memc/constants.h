#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>

namespace memc {
namespace constants {

inline constexpr std::string_view PROC_ROOT = "/proc";
inline constexpr std::string_view PROC_MAPS_SUFFIX = "/maps";
inline constexpr std::string_view PROC_SMAPS_SUFFIX = "/smaps";
inline constexpr std::string_view PROC_COMM_SUFFIX = "/comm";
inline constexpr std::string_view REGION_HEAP = "[heap]";
inline constexpr std::string_view REGION_STACK_PREFIX = "[stack";
inline constexpr std::string_view REGION_VDSO = "[vdso]";
inline constexpr std::string_view REGION_VVAR = "[vvar]";
inline constexpr std::string_view REGION_VSYSCALL = "[vsyscall]";
inline constexpr std::string_view SHARED_LIB_EXTENSION = ".so";
inline constexpr std::string_view SMAPS_KEY_SIZE = "Size";
inline constexpr std::string_view SMAPS_KEY_RSS = "Rss";
inline constexpr std::string_view SMAPS_KEY_PSS = "Pss";
inline constexpr std::string_view SMAPS_KEY_SHARED_CLEAN = "Shared_Clean";
inline constexpr std::string_view SMAPS_KEY_SHARED_DIRTY = "Shared_Dirty";
inline constexpr std::string_view SMAPS_KEY_PRIVATE_CLEAN = "Private_Clean";
inline constexpr std::string_view SMAPS_KEY_PRIVATE_DIRTY = "Private_Dirty";
inline constexpr std::string_view SMAPS_KEY_SWAP = "Swap";
inline constexpr std::size_t PERM_EXECUTE_INDEX = 2;
inline constexpr char PERM_EXECUTE_CHAR = 'x';
inline constexpr std::size_t PERM_MIN_LENGTH = 3;
inline constexpr uint64_t BYTES_PER_KB = 1024;
inline constexpr uint32_t DEFAULT_INTERVAL_MS = 1000;
inline constexpr int POLL_SLEEP_MS = 50;
inline constexpr int JSON_INDENT = 2;
inline constexpr std::string_view UNKNOWN_PROCESS_NAME = "unknown";
inline constexpr std::string_view LOG_PREFIX = "[memc]";

} // namespace constants

/**
 * @brief Builds a /proc/<pid>/<suffix> path string.
 *
 * @param pid  The process ID.
 * @param suffix  The file suffix (e.g. constants::PROC_MAPS_SUFFIX).
 * @return std::string  The full proc path.
 */
inline std::string proc_path(pid_t pid, std::string_view suffix) {
    return std::string(constants::PROC_ROOT) + "/" + std::to_string(pid) + std::string(suffix);
}

} // namespace memc
