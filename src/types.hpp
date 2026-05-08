// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#pragma once
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <thread>
#include <vector>

// Defines main types and constants used throughout Prime Differences.

const std::string VERSION_INFORMATION =
    "Prime Differences v 1.6.0\nCopyright 2025-2026 David Paul\nReleased under a BSD-2-Clause "
    "license";

// The data type to use for processing.
// We use 64-bit words to maximize throughput on x64 systems, though it should
// be possible to change this to other values (e.g., uint32_t or __uint128_t)
// with minimal changes.
using value_type = uint64_t;

// Shorthands for common data types.
using vector_type = std::vector<value_type>;
using span_type = std::span<value_type>;
using span_const_type = std::span<const value_type>;

// Basic data values that can be calculated at compile time.
constexpr value_type ZERO = 0;
constexpr value_type ONE = 1;
constexpr value_type NOT_ZERO = static_cast<value_type>(~ZERO);  // all ones

constexpr value_type BYTES_PER_WORD = sizeof(value_type);
constexpr value_type BITS_PER_WORD = BYTES_PER_WORD * CHAR_BIT;
constexpr value_type BITS_PER_WORD_MINUS_ONE = BITS_PER_WORD - ONE;

// Global flag modified by the signal handler (SIGINT) to trigger
// a graceful shutdown (save state and exit).
extern std::atomic_flag KEEP_RUNNING;

// The default limit to search for primes up to.
const value_type DEFAULT_PRIME_LIMIT = 1e10;

// Default number of threads to use.
// Note that this defaults to the number of logical cores, when it should
// probably be the number of physical processors for best performance.
const value_type DEFAULT_NUM_THREADS =
    std::max(ONE, static_cast<value_type>(std::thread::hardware_concurrency()));

// By default, we break up the range into this many tasks per thread to help
// ensure we don't have one thread doing nothing while there's still more work
// to do. 8 is arbitrary, but greater than 1 is preferable to help with load
// balancing.
constexpr value_type DEFAULT_TASKS_PER_THREAD = 8;

/// @brief A journal entry that specifies the number of primes (count) in the
/// range [start, end) and how long it took to calculate (cpu_time).
struct journal_entry {
  value_type start;
  value_type end;
  double cpu_time;
  vector_type counts;
};

// Default auto-save frequency (seconds) to ensure we don't lose too many
// calculations if the program is interrupted and restarted.
constexpr double DEFAULT_CHECKPOINT_FREQUENCY = 10 * 60;

// Default name of the file to save the journal to.
const std::string DEFAULT_JOURNAL_FILENAME = "prime_counts";

// Default name of the file to save the report to.
const std::string DEFAULT_REPORT_FILENAME = "prime_counts.csv";

/// @brief A discrete unit of work (task) and its current progress.
struct worker_state {
  std::string journal_filename;  // The name of the file to record completed
                                 // tasks. Copied from the global state so the
                                 // worker is self-contained.
  std::string worker_filename;   // The name of the file to use for checkpointing
                                 // the incomplete task.
  value_type start;              // The inclusive start of the range assignment.
  value_type end;                // The exclusive end of the range assignment.
  value_type current_position;   // Where the worker is up to. On resume, work
                                 // begins at current_position, not start.
  vector_type differences;       // The differences to count (e.g., 0, 2, 4, 6, ...). Copied
                                 // from global state so the worker is self-contained.
  vector_type current_counts;    // Running totals of difference counts found in
                                 // the range [start, current_position).
  double cpu_time;               // Execution time in seconds.
  double checkpoint_frequency;   // How often (seconds) to write to disk. Copied from
                                 // global state so the worker is self-contained.
  std::string parameters;        // Parameters for the worker (implementation dependent).
};

/// @brief The global state to be calculated.
struct global_state {
  std::string journal_filename;       // Where to store all completed counts, and the base of
                                      // the filename for all workers' checkpoint files.
  std::string report_filename;        // Where to store CSV of results on completion
  value_type start;                   // The inclusive start of the range to be counted.
  vector_type differences;            // The differences to count (e.g., 0, 2, 4, 6, ...).
  vector_type report_levels;          // The specific levels at which we want to report a count
                                      // (these are inclusive, so e.g., having 101 means report
                                      // the counts for values less than or equal to 101). The
                                      // largest of these values is the limit of the count.
  value_type num_threads;             // The number of threads to include in the thread pool.
  value_type tasks_per_thread;        // The number of tasks to make for each thread.
  std::vector<worker_state> workers;  // The list of all tasks that need to be completed.
  double checkpoint_frequency;        // How often (seconds) to write to disk.
  std::string parameters;             // Parameters for each worker (implementation dependent).
};
