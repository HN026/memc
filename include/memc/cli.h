#pragma once

#include <memc/collector.h>

#include <string>
#include <sys/types.h>

namespace memc {

/**
 * @brief Holds all parsed command-line options for the memc CLI.
 *
 * Fields:
 * - pid: Target process ID (0 if --all mode).
 * - all_mode: If true, snapshot all processes on the system.
 * - skip_kernel: If true, skip kernel threads with no user-space memory.
 * - count: Number of samples to take (1 = single, 0 = continuous).
 * - output_file: Path to write JSON output (empty = stdout).
 * - collector_config: Configuration forwarded to DataCollector.
 * - show_help: If true, print usage and exit.
 * - show_version: If true, print version and exit.
 * - parse_error: If true, an error was encountered during parsing.
 * - error_message: Description of the parse error, if any.
 */
struct CLIOptions {
  pid_t pid = 0;
  bool all_mode = false;
  bool skip_kernel = false;
  int count = 1;
  std::string output_file;
  DataCollector::Config collector_config;

  bool show_help = false;
  bool show_version = false;
  bool parse_error = false;
  std::string error_message;
};

/**
 * @brief Parses command-line arguments into a CLIOptions struct.
 *
 * @param argc The argument count.
 * @param argv The argument vector.
 * @return CLIOptions The parsed options. Check show_help, show_version,
 * and parse_error fields to determine early-exit conditions.
 */
CLIOptions parse_args(int argc, char* argv[]);

/**
 * @brief Prints the usage/help message to stderr.
 *
 * @param prog The program name (argv[0]).
 */
void print_usage(const char* prog);

} // namespace memc
