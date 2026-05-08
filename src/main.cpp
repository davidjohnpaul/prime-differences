// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <limits>
#include <print>
#include <stdexcept>
#include <string>
#include <thread>

#include "parsing.hpp"
#include "state.hpp"
#include "sys_info.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "worker.hpp"
#include "worker_impl.hpp"

// Prime Differences is a high-performance, multi-threaded C++ utility designed
// to count occurrences of specific prime differences over massive ranges.

/// @brief Prints the CLI help manual.
/// @return EXIT_FAILURE (convention for usage errors).
int usage(const std::string& cmd, const std::string message = {}) {
  if (!message.empty()) {
    std::println("{}\n", message);
  }
  std::println(
      "Usage: {} [--help | -h | /?] | [--version | -v] | [-d DIFFERENCES] "
      "[-s START] [-l REPORT_LEVELS] [-t THREADS] [-k TASKS] "
      "[-j JOURNAL_FILENAME] [-r REPORT_FILENAME] [-c CHECKPOINT_FREQUENCY] "
      "[-p PARAMETERS]",
      cmd);
  std::println("Where:");
  std::println("\t{:<30}\tdisplays usage information", "--help | -h | /?");
  std::println("\t{:<30}\tdisplays version information and system defaults", "--version | -v");
  std::println(
      "\t{:<30}\tis the name of a file that contains "
      "comma-separated lists of differences to count the number of "
      "primes for, or a comma-separated list of differences (if none "
      "are specified, defaults to primes and twin primes (i.e., \"0,2\"))",
      "DIFFERENCES");
  std::println(
      "\t{:<30}\tis where to start the prime search from (inclusive, "
      "defaults to 0)",
      "START");
  std::println(
      "\t{:<30}\tis the name of a file that contains comma-separated "
      "lists of levels at which the count should be output, or a "
      "comma-separated list of report levels for which the count should be "
      "output (inclusive. If none are specified, defaults to just {})",
      "REPORT_LEVELS", DEFAULT_PRIME_LIMIT);
  std::println(
      "\t{:<30}\tis the number of threads to use (defaults to number "
      "of cores, {} on this system). For best performance, set -t to "
      "your PHYSICAL core count, not logical threads.",
      "THREADS", DEFAULT_NUM_THREADS);
  std::println(
      "\t{:<30}\tis the number of tasks to create for each thread "
      "(defaults to {}). Increase to better balance the load between "
      "threads, especially when report levels are far apart.",
      "TASKS", DEFAULT_TASKS_PER_THREAD);
  std::println(
      "\t{:<30}\tis the filename where results will be written "
      "(defaults to {}). A value of \"\" means no journal output will "
      "be generated (useful for checking timing, but counts will not "
      "be output). A csv file named JOURNAL_FILENAME.csv will be created on "
      "successful completion only if journal output is generated.",
      "JOURNAL_FILENAME", DEFAULT_JOURNAL_FILENAME);
  std::println(
      "\t{:<30}\tis the filename where final results will be reported "
      "if journal output is generated.",
      "REPORT_FILENAME", DEFAULT_REPORT_FILENAME);
  std::println(
      "\t{:<30}\tis the number of seconds to wait before saving a checkpoint "
      "for each thread (defaults to {}). A value of 0 means no checkpointing.",
      "CHECKPOINT_FREQUENCY", DEFAULT_CHECKPOINT_FREQUENCY);
  std::println("\t{} {}", "PARAMETERS", Worker::get_parameter_description());
  std::println("");
  std::println(
      "Note that the program creates other files to store state, so you should "
      "be able to resume if interrupted without losing too many calculations "
      "by just re-running the previous command.");
  std::println(
      "Each such file will have a name starting with JOURNAL_FILENAME "
      "and will be saved roughly every CHECKPOINT_FREQUENCY seconds.");
  std::println("");
  std::println("Examples:");
  std::println("");
  std::println("\tCount twin primes up to 10 billion:");
  std::println("\t\t{} -d 2 -l 10000000000", cmd);
  std::println("");
  std::println("\tCount twin, cousin, and sexy primes with multiple report levels:");
  std::println("\t\t{} -d 2,4,6 -l 1000000,10000000,100000000,1000000000", cmd);
  std::println("");
  std::println("\tCount all primes in range 1B-2B using 8 threads:");
  std::println("\t\t{} -s 1000000000 -d 0 -l 2000000000 -t 8", cmd);
  std::println("");
  std::println(
      "\tUse differences from a file, checkpoint every 5 minutes, "
      "store results in results/run1:");
  std::println("\t\t{} -d diffs.txt -l 10000000000 -c 300 -j results/run1", cmd);
  std::println("");
  std::println("\tQuick test (should finish quickly):");
  std::println("\t\t{} -d 2,4,6 -l 1000000 - j \"\" -c 0", cmd);
  std::println("");
  return EXIT_FAILURE;
}

// Global flag monitored by all worker threads.
// When set to false, workers finish (and checkpoint) their current work and
// exit.
std::atomic_flag KEEP_RUNNING;

/// @brief Handles Ctrl+C (SIGINT).
/// Need to use the write system call because we cannot use functions that
/// require a lock in an interrupt context.
void signal_handler(int signal) {
  KEEP_RUNNING.clear(std::memory_order_relaxed);
  // Async-safe write to stdout (File Descriptor 1)
  const char msg[] = "\nSignal received. Finishing current work...\n";
  (void)write(1, msg, sizeof(msg) - 1);
}

/// @brief Constructs the global state from command line arguments.
///
/// If explicit parameters are given, use them. Otherwise, use defaults.
/// @throw runtime_error if the global state or any parameters are invalid.
[[nodiscard]] global_state parse_args(int argc, char* argv[]) {
  global_state result;
  result.differences = {0, 2};  // Primes and twin primes by default
  result.start = 0;
  result.report_levels = {DEFAULT_PRIME_LIMIT};
  result.num_threads = DEFAULT_NUM_THREADS;
  result.tasks_per_thread = DEFAULT_TASKS_PER_THREAD;
  result.journal_filename = DEFAULT_JOURNAL_FILENAME;
  result.report_filename = DEFAULT_REPORT_FILENAME;
  result.checkpoint_frequency = DEFAULT_CHECKPOINT_FREQUENCY;
  result.parameters = "";  // No worker parameters by default
  vector_type differences = result.differences;
  vector_type report_levels = result.report_levels;
  size_t i = 1;  // 0 is the command
  while (i < argc) {
    std::string arg = argv[i];
    if ("--help" == arg || "-h" == arg || "/?" == arg) {
      // Use error handling to display version and usage information
      usage(argv[0], std::format("{}\nVariant: {}", VERSION_INFORMATION, SELECTED_VARIANT_NAME));
      std::exit(EXIT_SUCCESS);
    } else if ("--version" == arg || "-v" == arg) {
      // Use error handling to display version, system, and usage information
      cache_sizes caches = get_cache_sizes();
      usage(
          argv[0],
          std::format("{}\nVariant: {}\n\nNumber of cores: {}\nL1 cache size: {} ({} KB)\nL2 cache "
                      "size: {} ({} KB)\nL3 cache size: {} ({} KB)",
                      VERSION_INFORMATION, SELECTED_VARIANT_NAME, DEFAULT_NUM_THREADS, caches.l1,
                      caches.l1 / 1024, caches.l2, caches.l2 / 1024, caches.l3, caches.l3 / 1024));
      std::exit(EXIT_SUCCESS);
    } else if ("-d" == arg) {  // Set differences
      i++;
      if (i >= argc) {
        throw runtime_error("Missing DIFFERENCES command line parameter");
      }
      std::string val = argv[i];
      try {
        differences = parse_vector_type_from_file(val);
      } catch (const std::runtime_error& e) {
        try {
          differences = parse_vector_type(val);
        } catch (const std::runtime_error& ee) {
          throw e;
        }
      }
    } else if ("-s" == arg) {  // Set start
      i++;
      if (i >= argc) {
        throw runtime_error("Missing START command line argument");
      }
      std::string val = argv[i];
      result.start = parse_value_type(val);
    } else if ("-l" == arg) {  // Set report levels
      i++;
      if (i >= argc) {
        throw runtime_error("Missing REPORT_LEVELS command line parameter");
      }
      std::string val = argv[i];
      try {
        report_levels = parse_vector_type_from_file(val);
      } catch (const std::runtime_error& e) {
        try {
          report_levels = parse_vector_type(val);
        } catch (const std::runtime_error& ee) {
          throw e;
        }
      }
    } else if ("-t" == arg) {  // Set number of threads
      i++;
      if (i >= argc) {
        throw runtime_error("Missing THREADS command line parameter");
      }
      std::string val = argv[i];
      result.num_threads = parse_value_type(val);
    } else if ("-k" == arg) {  // Set number of tasks per thread
      i++;
      if (i >= argc) {
        throw runtime_error("Missings TASKS command line parameter");
      }
      std::string val = argv[i];
      result.tasks_per_thread = parse_value_type(val);
    } else if ("-j" == arg) {  // Set journal filename
      i++;
      if (i >= argc) {
        throw runtime_error("Missing JOURNAL_FILENAME command line argument");
      }
      std::string val = argv[i];
      result.journal_filename = val;
    } else if ("-r" == arg) {  // Set report filename
      i++;
      if (i >= argc) {
        throw runtime_error("Missing REPORT_FILENAME command line argument");
      }
      std::string val = argv[i];
      result.report_filename = val;
    } else if ("-c" == arg) {  // Set checkpoint frequency
      i++;
      if (i >= argc) {
        throw runtime_error("Missing CHECKPOINT_FREQUENCY command line parameter");
      }
      std::string val = argv[i];
      result.checkpoint_frequency = std::stod(val);
    } else if ("-p" == arg) {  // Set worker parameters
      i++;
      if (i >= argc) {
        throw runtime_error("Missing PARAMETERS command line parameter");
      }
      std::string val = argv[i];
      result.parameters = val;
    } else {  // Unknown option
      throw runtime_error(std::format("Unknown command line parameter: {}", arg));
    }
    i++;
  }

  // Ensure there are differences to count, and that they are sorted and
  // unique
  if (differences.empty()) {
    differences = result.differences;
  }
  std::ranges::sort(differences);
  auto [first_diff, last_diff] = std::ranges::unique(differences);
  differences.erase(first_diff, last_diff);
  result.differences = differences;

  // Ensure there are report levels to record, and that they are sorted and
  // unique
  if (report_levels.empty()) {
    report_levels = result.report_levels;
  }
  std::ranges::sort(report_levels);
  auto [first_level, last_level] = std::ranges::unique(report_levels);
  report_levels.erase(first_level, last_level);
  if (report_levels.back() >= std::numeric_limits<value_type>::max()) {
    throw runtime_error(
        "ERROR: Maximum supported report level exceeded. "
        "Please reduce the largest report level.");
  }
  result.report_levels = report_levels;

  // Ensure we have a valid range
  if (result.start > result.report_levels.back()) {
    throw runtime_error("ERROR: start must be less than the largest report_level");
  }

  if (std::numeric_limits<value_type>::max() - result.differences.back() <=
      result.report_levels.back()) {
    throw runtime_error(
        std::format("ERROR: final report level plus maximum "
                    "difference must be less than {}",
                    std::numeric_limits<value_type>::max() - ONE));
  }

  // Ensure we have a valid number of threads
  if (result.num_threads == 0) {
    throw runtime_error("ERROR: At least one thread is required");
  }

  // Ensure we have a valid number of tasks per thread
  if (result.tasks_per_thread == 0) {
    throw runtime_error("ERROR: At least one task per thread is required");
  }

  if (std::numeric_limits<value_type>::max() / result.num_threads < result.tasks_per_thread) {
    throw runtime_error(
        "ERROR: Too many tasks! Please reduce the number of "
        "threads or the number of tasks per thread.");
  }

  // Ensure journal directory exists, or warn if no journal will be created
  if (result.journal_filename.empty()) {
    log_message(std::format("WARN: Journalling is disabled, so no counts can be output."));
  } else {
    ensure_directory_exists(result.journal_filename);
  }

  // Ensure report directory exists, or warn if no report will be created
  if (!result.journal_filename.empty()) {
    if (result.report_filename.empty()) {
      log_message(std::format("WARN: Reporting is disabled, so no report will be generated."));
    } else {
      ensure_directory_exists(result.report_filename);
    }
  }

  // Warn if checkpointing is disabled
  if (result.checkpoint_frequency == 0) {
    log_message("WARN: Checkpointing is disabled.");
  }

  result.workers = get_worker_states(result);
  return result;
}

/// @brief Output information about the calculations to be performed.
void output_details(global_state& state) {
  std::println("{}\n", VERSION_INFORMATION);
  std::print(
      "Counting prime pairs from {} to {} with differences of {} and reporting "
      "at levels {} using {} thread(s) with {} task(s) per thread",
      state.start, state.report_levels.back(), vector_type_to_string(state.differences),
      vector_type_to_string(state.report_levels), state.num_threads, state.tasks_per_thread);
  if (state.checkpoint_frequency > 0) {
    std::print(", checkpointing every {} second(s)", state.checkpoint_frequency);
  }
  if (!state.journal_filename.empty()) {
    std::print(" and outputting to {}", state.journal_filename);
    if (!state.report_filename.empty()) {
      std::print(" with final report in {}", state.report_filename);
    }
  }
  std::println(".\n");
}

/// @brief Perform the calculations specified in the given state.
/// Create and launch a thread pool where each thread grabs an incomplete task,
/// completes it, and repeats until all tasks are finalised. Then outputs a
/// report if all tasks completed successfully.
void count_primes_with_differences(global_state state) {
  std::vector<std::thread> workers;
  workers.reserve(state.num_threads);

  // Points to the index of the next unprocessed task in state.workers.
  std::atomic<size_t> next_task_index{0};

  // Each thread in the pool
  auto pool_worker = [&state, &next_task_index]() {
    while (true) {
      // Atomically fetches the index of the task to complete
      size_t task_index = next_task_index.fetch_add(1, std::memory_order_acquire);

      // Only continue if there is a task to process
      if (task_index >= state.workers.size()) {
        return;
      }

      // Process the current task
      worker_state& task = state.workers[task_index];
      run_worker(task);
    }
  };

  // Launch threads
  for (size_t i = 0; i < state.num_threads; i++) {
    workers.emplace_back(pool_worker);
  }

  // Wait for all threads to finish
  for (auto& thread : workers) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  // Ensure all tasks completed successfully.
  bool all_workers_completed = true;
  for (size_t i = 0; i < state.workers.size(); i++) {
    auto& worker = state.workers[i];
    if (worker.current_position < worker.end) {
      all_workers_completed = false;
      break;
    }
  }

  // Aggregate and report on results.
  if (all_workers_completed) {
    if (!state.journal_filename.empty()) {
      std::string report = get_report_string(state);
      std::println("{}", report);
      if (!state.report_filename.empty()) {
        output_to_file(state.report_filename, report);
        std::println("Report written to {}", state.report_filename);
      }
    } else {
      std::println(
          "All workers completed successfully but journalling was "
          "disabled, so no output will be produced.");
    }
  } else {
    std::println(
        "Not all workers have completed successfully. Repeating the "
        "previous command should resume any incomplete tasks.");
  }
}

/// @brief Program entry point.
/// See usage() for details of command line arguments.
int main(int argc, char* argv[]) {
  KEEP_RUNNING.test_and_set();
  std::string cmd = argv[0];
  try {
    global_state state = parse_args(argc, argv);
    output_details(state);
    std::signal(SIGINT, signal_handler);
    count_primes_with_differences(state);
  } catch (const std::runtime_error& e) {
    return usage(cmd, e.what());
  }
  return EXIT_SUCCESS;
}
