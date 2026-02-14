#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memc/cli.h>
#include <memc/version.h>

namespace memc {

/**
 * @brief Parses command-line arguments into a CLIOptions struct.
 *
 * Iterates through argv, setting flags and values on the returned
 * CLIOptions. Validates required argument values and sets parse_error
 * with a descriptive message on failure.
 *
 * @param argc The argument count.
 * @param argv The argument vector.
 * @return CLIOptions The parsed options.
 */
CLIOptions parse_args(int argc, char* argv[]) {
    CLIOptions opts;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            opts.show_help = true;
            return opts;
        } else if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            opts.show_version = true;
            return opts;
        } else if (std::strcmp(argv[i], "--all") == 0) {
            opts.all_mode = true;
        } else if (std::strcmp(argv[i], "--smaps") == 0) {
            opts.collector_config.use_smaps = true;
        } else if (std::strcmp(argv[i], "--skip-kernel") == 0) {
            opts.skip_kernel = true;
        } else if (std::strcmp(argv[i], "--compact") == 0) {
            opts.collector_config.pretty_json = false;
        } else if (std::strcmp(argv[i], "--output") == 0 || std::strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                opts.parse_error = true;
                opts.error_message = "Error: --output requires a filename";
                return opts;
            }
            opts.output_file = argv[++i];
        } else if (std::strcmp(argv[i], "--interval") == 0) {
            if (i + 1 >= argc) {
                opts.parse_error = true;
                opts.error_message = "Error: --interval requires a value";
                return opts;
            }
            opts.collector_config.interval_ms = std::atoi(argv[++i]);
            if (opts.collector_config.interval_ms <= 0) {
                opts.parse_error = true;
                opts.error_message = "Error: interval must be positive";
                return opts;
            }
        } else if (std::strcmp(argv[i], "--count") == 0) {
            if (i + 1 >= argc) {
                opts.parse_error = true;
                opts.error_message = "Error: --count requires a value";
                return opts;
            }
            opts.count = std::atoi(argv[++i]);
        } else if (opts.pid == 0 && !opts.all_mode) {
            opts.pid = std::atoi(argv[i]);
            if (opts.pid <= 0) {
                opts.parse_error = true;
                opts.error_message = std::string("Error: invalid PID '") + argv[i] + "'";
                return opts;
            }
        } else {
            opts.parse_error = true;
            opts.error_message = std::string("Error: unknown argument '") + argv[i] + "'";
            return opts;
        }
    }

    if (!opts.all_mode && opts.pid == 0) {
        opts.parse_error = true;
        opts.error_message = "Error: PID is required (or use --all)";
    }

    return opts;
}

/**
 * @brief Prints the usage/help message to stderr.
 *
 * @param prog The program name (argv[0]).
 */
void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <pid> [options]\n"
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
              << "  " << prog << " 1234                        # Single snapshot of PID 1234\n"
              << "  " << prog << " 1234 --smaps                # With detailed memory info\n"
              << "  " << prog << " --all --smaps               # All processes with smaps\n"
              << "  " << prog << " --all --output system.json   # Save to file\n"
              << "  " << prog << " 1234 --count 0 --interval 500  # Continuous, every 500ms\n"
              << "  " << prog << " $$                          # Monitor the current shell\n";
}

} // namespace memc
