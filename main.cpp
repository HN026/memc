/// memc — Memory Collector CLI
///
/// Usage:
///   memc <pid> [options]
///   memc --all [options]
///
/// Options:
///   --all           Snapshot ALL processes on the system
///   --smaps         Enable detailed smaps data (RSS, PSS, swap, etc.)
///   --interval <ms> Sampling interval in milliseconds (default: 1000)
///   --count <n>     Number of samples to take (default: 1, 0 = continuous)
///   --compact       Output compact JSON (default: pretty-printed)
///   --output <file> Write JSON to a file instead of stdout
///   --help          Show this help message

#include <memc/collector.h>
#include <memc/version.h>

#include <atomic>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static std::atomic<bool> g_running{true};

static void signal_handler(int /*sig*/) { g_running.store(false); }

/// Enumerate all numeric PIDs from /proc.
static std::vector<pid_t> enumerate_pids() {
  std::vector<pid_t> pids;
  DIR *dir = opendir("/proc");
  if (!dir)
    return pids;

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    // Only consider entries that are purely numeric (PIDs)
    const char *name = entry->d_name;
    bool is_pid = true;
    for (const char *p = name; *p; ++p) {
      if (!std::isdigit(static_cast<unsigned char>(*p))) {
        is_pid = false;
        break;
      }
    }
    if (is_pid && name[0] != '\0') {
      pids.push_back(static_cast<pid_t>(std::atoi(name)));
    }
  }
  closedir(dir);

  std::sort(pids.begin(), pids.end());
  return pids;
}

/// Read /proc/<pid>/comm to get the process name.
static std::string get_process_name(pid_t pid) {
  std::string path = "/proc/" + std::to_string(pid) + "/comm";
  std::ifstream ifs(path);
  std::string name;
  if (ifs.is_open() && std::getline(ifs, name)) {
    // Trim trailing newline
    while (!name.empty() && (name.back() == '\n' || name.back() == '\r')) {
      name.pop_back();
    }
    return name;
  }
  return "unknown";
}

static void print_usage(const char *prog) {
  std::cerr
      << "Usage: " << prog << " <pid> [options]\n"
      << "       " << prog << " --all [options]\n"
      << "\n"
      << "Memory region data collector for Linux processes.\n"
      << "Reads /proc/<pid>/maps (and optionally smaps) and outputs JSON.\n"
      << "\n"
      << "Options:\n"
      << "  --all            Snapshot ALL processes on the system\n"
      << "  --smaps          Enable detailed smaps data (RSS, PSS, swap, "
         "etc.)\n"
      << "  --interval <ms>  Sampling interval in milliseconds (default: "
         "1000)\n"
      << "  --count <n>      Number of samples to take (default: 1, 0 = "
         "continuous)\n"
      << "  --compact        Output compact JSON (default: pretty-printed)\n"
      << "  --output <file>  Write JSON to a file instead of stdout\n"
      << "  --skip-kernel    Skip kernel threads with no user-space memory\n"
      << "  --version        Show version information\n"
      << "  --help           Show this help message\n"
      << "\n"
      << "Examples:\n"
      << "  " << prog
      << " 1234                        # Single snapshot of PID 1234\n"
      << "  " << prog
      << " 1234 --smaps                # With detailed memory info\n"
      << "  " << prog
      << " --all --smaps               # All processes with smaps\n"
      << "  " << prog << " --all --output system.json   # Save to file\n"
      << "  " << prog
      << " 1234 --count 0 --interval 500  # Continuous, every 500ms\n"
      << "  " << prog
      << " $$                          # Monitor the current shell\n";
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  // Parse arguments
  pid_t pid = 0;
  bool all_mode = false;
  bool skip_kernel = false;
  memc::DataCollector::Config config;
  int count = 1;
  std::string output_file;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--help") == 0 ||
        std::strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    } else if (std::strcmp(argv[i], "--version") == 0 ||
               std::strcmp(argv[i], "-v") == 0) {
      std::cout << "memc " << MEMC_VERSION_STRING << std::endl;
      return 0;
    } else if (std::strcmp(argv[i], "--all") == 0) {
      all_mode = true;
    } else if (std::strcmp(argv[i], "--smaps") == 0) {
      config.use_smaps = true;
    } else if (std::strcmp(argv[i], "--skip-kernel") == 0) {
      skip_kernel = true;
    } else if (std::strcmp(argv[i], "--compact") == 0) {
      config.pretty_json = false;
    } else if (std::strcmp(argv[i], "--output") == 0 ||
               std::strcmp(argv[i], "-o") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --output requires a filename\n";
        return 1;
      }
      output_file = argv[++i];
    } else if (std::strcmp(argv[i], "--interval") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --interval requires a value\n";
        return 1;
      }
      config.interval_ms = std::atoi(argv[++i]);
      if (config.interval_ms <= 0) {
        std::cerr << "Error: interval must be positive\n";
        return 1;
      }
    } else if (std::strcmp(argv[i], "--count") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --count requires a value\n";
        return 1;
      }
      count = std::atoi(argv[++i]);
    } else if (pid == 0 && !all_mode) {
      pid = std::atoi(argv[i]);
      if (pid <= 0) {
        std::cerr << "Error: invalid PID '" << argv[i] << "'\n";
        return 1;
      }
    } else {
      std::cerr << "Error: unknown argument '" << argv[i] << "'\n";
      print_usage(argv[0]);
      return 1;
    }
  }

  if (!all_mode && pid == 0) {
    std::cerr << "Error: PID is required (or use --all)\n";
    print_usage(argv[0]);
    return 1;
  }

  // Install signal handlers for graceful shutdown
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // Helper to write output to file or stdout
  auto write_output = [&](const std::string &json_str) {
    if (!output_file.empty()) {
      std::ofstream ofs(output_file);
      if (!ofs.is_open()) {
        std::cerr << "Error: could not open '" << output_file
                  << "' for writing\n";
        return false;
      }
      ofs << json_str << std::endl;
      std::cerr << "Written to " << output_file << "\n";
      return true;
    }
    std::cout << json_str << std::endl;
    return true;
  };

  // ─── ALL PROCESSES MODE ─────────────────────────────────────────
  if (all_mode) {
    auto pids = enumerate_pids();
    std::cerr << "Scanning " << pids.size() << " processes"
              << (config.use_smaps ? " (with smaps)" : "") << "...\n";

    nlohmann::ordered_json result;
    auto now = std::chrono::system_clock::now();
    result["timestamp_ms"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch())
            .count();
    result["process_count"] = 0;
    result["processes"] = nlohmann::ordered_json::array();
    result["skipped_count"] = 0;
    result["skipped_processes"] = nlohmann::ordered_json::array();

    int collected = 0;
    int skipped = 0;

    for (pid_t p : pids) {
      if (!g_running.load())
        break;

      memc::DataCollector collector(p, config);
      auto snapshot = collector.collect_once();
      if (!snapshot) {
        skipped++;
        nlohmann::ordered_json skipped_entry;
        skipped_entry["pid"] = p;
        skipped_entry["name"] = get_process_name(p);
        result["skipped_processes"].push_back(std::move(skipped_entry));
        continue;
      }

      // Skip kernel threads if requested
      if (skip_kernel && snapshot->regions.empty()) {
        continue;
      }

      nlohmann::ordered_json proc_entry;
      proc_entry["pid"] = p;
      proc_entry["name"] = get_process_name(p);
      nlohmann::ordered_json snap_j;
      memc::to_json(snap_j, *snapshot);
      proc_entry["snapshot"] = std::move(snap_j);

      result["processes"].push_back(std::move(proc_entry));
      collected++;
    }

    result["process_count"] = collected;
    result["skipped_count"] = skipped;

    std::cerr << "Collected " << collected << " process snapshots (" << skipped
              << " skipped due to permissions).\n";

    std::string json_str = config.pretty_json ? result.dump(2) : result.dump();
    write_output(json_str);

    return 0;
  }

  // ─── SINGLE PROCESS MODE ────────────────────────────────────────
  memc::DataCollector collector(pid, config);

  if (count == 1) {
    // Single snapshot mode
    auto snapshot = collector.collect_once();
    if (!snapshot) {
      std::cerr << "Error: failed to read /proc/" << pid << "/maps\n"
                << "Check that the process exists and you have permission.\n";
      return 1;
    }
    write_output(collector.to_json(*snapshot));
  } else {
    // Periodic sampling mode
    bool continuous = (count == 0);
    int samples_taken = 0;

    std::cerr << "Sampling PID " << pid << " every " << config.interval_ms
              << "ms" << (config.use_smaps ? " (with smaps)" : "")
              << (continuous ? " (Ctrl+C to stop)" : "") << "...\n";

    while (g_running.load()) {
      auto snapshot = collector.collect_once();
      if (!snapshot) {
        std::cerr << "Warning: failed to read process " << pid
                  << " — it may have exited.\n";
        break;
      }

      std::cout << collector.to_json(*snapshot) << std::endl;
      samples_taken++;

      if (!continuous && samples_taken >= count) {
        break;
      }

      // Sleep with interruption check
      auto deadline = std::chrono::steady_clock::now() +
                      std::chrono::milliseconds(config.interval_ms);
      while (g_running.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }

    std::cerr << "Collected " << samples_taken << " snapshot(s).\n";
  }

  return 0;
}
