#pragma once

#include <string>
#include <sys/types.h>
#include <vector>

namespace memc {

/**
 * @brief Enumerates all numeric PIDs from /proc.
 *
 * Scans the /proc directory for entries whose names are purely numeric,
 * interpreting each as a process ID.
 *
 * @return std::vector<pid_t> A sorted list of discovered PIDs.
 */
std::vector<pid_t> enumerate_pids();

/**
 * @brief Reads /proc/<pid>/comm to get the process name.
 *
 * @param pid The process ID.
 * @return std::string The process name, or "unknown" if not found.
 */
std::string get_process_name(pid_t pid);

} // namespace memc
