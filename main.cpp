/**
 * memc — Memory Collector CLI
 *
 * Entry point for the memc command-line tool.
 * See `memc --help` for usage details.
 */

#include <memc/cli.h>
#include <memc/collector.h>
#include <memc/process_utils.h>
#include <memc/version.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>

static std::atomic<bool> g_running{true};

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 *
 * Sets the global running flag to false so sampling loops can exit gracefully.
 *
 * @param sig The signal number (unused).
 */
static void signal_handler(int /*sig*/) { g_running.store(false); }

/**
 * @brief Writes a JSON string to the configured output destination.
 *
 * If an output file is specified, writes to that file. Otherwise, writes
 * to stdout.
 *
 * @param json_str The JSON string to write.
 * @param output_file Path to the output file (empty = stdout).
 * @return true if the write was successful, false otherwise.
 */
static bool write_output(const std::string &json_str,
                         const std::string &output_file) {
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
}

/**
 * @brief Runs the all-processes scan mode.
 *
 * Enumerates every PID on the system, collects a snapshot for each,
 * and writes the combined result as a single JSON object.
 *
 * @param opts The parsed CLI options.
 * @return int 0 on success.
 */
static int run_all_mode(const memc::CLIOptions &opts) {
  auto pids = memc::enumerate_pids();
  std::cerr << "Scanning " << pids.size() << " processes"
            << (opts.collector_config.use_smaps ? " (with smaps)" : "")
            << "...\n";

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

    memc::DataCollector collector(p, opts.collector_config);
    auto snapshot = collector.collect_once();
    if (!snapshot) {
      skipped++;
      nlohmann::ordered_json skipped_entry;
      skipped_entry["pid"] = p;
      skipped_entry["name"] = memc::get_process_name(p);
      result["skipped_processes"].push_back(std::move(skipped_entry));
      continue;
    }

    if (opts.skip_kernel && snapshot->regions.empty()) {
      continue;
    }

    nlohmann::ordered_json proc_entry;
    proc_entry["pid"] = p;
    proc_entry["name"] = memc::get_process_name(p);
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

  std::string json_str =
      opts.collector_config.pretty_json ? result.dump(2) : result.dump();
  write_output(json_str, opts.output_file);

  return 0;
}

/**
 * @brief Runs the single-PID mode (one-shot or periodic sampling).
 *
 * If count is 1, takes a single snapshot. Otherwise, samples at the
 * configured interval until the count is reached or the user interrupts.
 *
 * @param opts The parsed CLI options.
 * @return int 0 on success, 1 on failure.
 */
static int run_single_pid(const memc::CLIOptions &opts) {
  memc::DataCollector collector(opts.pid, opts.collector_config);

  if (opts.count == 1) {
    auto snapshot = collector.collect_once();
    if (!snapshot) {
      std::cerr << "Error: failed to read /proc/" << opts.pid << "/maps\n"
                << "Check that the process exists and you have permission.\n";
      return 1;
    }
    write_output(collector.to_json(*snapshot), opts.output_file);
  } else {
    bool continuous = (opts.count == 0);
    int samples_taken = 0;

    std::cerr << "Sampling PID " << opts.pid << " every "
              << opts.collector_config.interval_ms << "ms"
              << (opts.collector_config.use_smaps ? " (with smaps)" : "")
              << (continuous ? " (Ctrl+C to stop)" : "") << "...\n";

    while (g_running.load()) {
      auto snapshot = collector.collect_once();
      if (!snapshot) {
        std::cerr << "Warning: failed to read process " << opts.pid
                  << " — it may have exited.\n";
        break;
      }

      std::cout << collector.to_json(*snapshot) << std::endl;
      samples_taken++;

      if (!continuous && samples_taken >= opts.count) {
        break;
      }

      auto deadline =
          std::chrono::steady_clock::now() +
          std::chrono::milliseconds(opts.collector_config.interval_ms);
      while (g_running.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }

    std::cerr << "Collected " << samples_taken << " snapshot(s).\n";
  }

  return 0;
}

/**
 * @brief Entry point for the memc CLI.
 *
 * Parses arguments, sets up signal handlers, and dispatches to
 * either all-process scan or single-PID mode.
 *
 * @param argc The argument count.
 * @param argv The argument vector.
 * @return int 0 on success, 1 on error.
 */
int main(int argc, char *argv[]) {
  if (argc < 2) {
    memc::print_usage(argv[0]);
    return 1;
  }

  auto opts = memc::parse_args(argc, argv);

  if (opts.show_help) {
    memc::print_usage(argv[0]);
    return 0;
  }
  if (opts.show_version) {
    std::cout << "memc " << MEMC_VERSION_STRING << std::endl;
    return 0;
  }
  if (opts.parse_error) {
    std::cerr << opts.error_message << "\n";
    memc::print_usage(argv[0]);
    return 1;
  }

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  if (opts.all_mode) {
    return run_all_mode(opts);
  }
  return run_single_pid(opts);
}
