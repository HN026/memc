#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <memc/process_utils.h>
#include <memory>
#include <string>

namespace memc {

/**
 * @brief Enumerates all numeric PIDs from /proc.
 *
 * Scans the /proc directory for entries whose names are purely numeric,
 * interpreting each as a process ID.
 *
 * @return std::vector<pid_t> A sorted list of discovered PIDs.
 */
std::vector<pid_t> enumerate_pids() {
    std::vector<pid_t> pids;
    auto dir = std::unique_ptr<DIR, int (*)(DIR*)>(opendir("/proc"), closedir);
    if (!dir)
        return pids;

    struct dirent* entry;
    while ((entry = readdir(dir.get())) != nullptr) {
        const char* name = entry->d_name;
        bool is_pid = true;
        for (const char* p = name; *p; ++p) {
            if (!std::isdigit(static_cast<unsigned char>(*p))) {
                is_pid = false;
                break;
            }
        }
        if (is_pid && name[0] != '\0') {
            pids.push_back(static_cast<pid_t>(std::atoi(name)));
        }
    }

    std::sort(pids.begin(), pids.end());
    return pids;
}

/**
 * @brief Reads /proc/<pid>/comm to get the process name.
 *
 * @param pid The process ID.
 * @return std::string The process name, or "unknown" if not found.
 */
std::string get_process_name(pid_t pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream ifs(path);
    std::string name;
    if (ifs.is_open() && std::getline(ifs, name)) {
        while (!name.empty() && (name.back() == '\n' || name.back() == '\r')) {
            name.pop_back();
        }
        return name;
    }
    return "unknown";
}

} // namespace memc
