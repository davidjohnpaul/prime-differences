// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "state.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

#include "parsing.hpp"
#include "types.hpp"
#include "utils.hpp"

// Mutex to ensure only one thread is writing to the journal at a time.
static std::mutex journal_mutex;

/// @brief Append the completed worker_state information to the journal.
void append_completion_record(const worker_state& state) {
  if (state.journal_filename.empty()) {
    return;
  }
  // Ensure exclusive access to journal file
  std::lock_guard<std::mutex> lock(journal_mutex);

  // Output to string, then to file
  std::ostringstream ss;
  //  Start, End, Time, Counts...
  ss << std::format("{},{},{}", state.start, state.end, state.cpu_time);
  for (const auto count : state.current_counts) {
    ss << std::format(",{}", count);
  }
  ss << "\n";
  std::string line = ss.str();

  std::ofstream out;

  // Set exceptions to throw if bad bit (hardware error) or fail bit (logic
  // error) occurs
  out.exceptions(std::ofstream::failbit | std::ofstream::badbit);

  try {
    out.open(state.journal_filename, std::ios::app);
    out << line;
    // Flush to ensure it hits OS buffer.
    // Note that this doesn't necessarily mean the data is on disk, but the
    // worker state should still be available in the checkpoint fileif such a
    // failure occurs, so nothing should be lost.
    out.flush();
  } catch (const std::ios_base::failure& e) {
    throw runtime_error(std::format("Failed to append to journal {}. Work NOT saved. Error: {}",
                                    state.journal_filename, e.what()));
  }
}

/// @brief Generates a list of ranges we need the counts for.
/// Generates num_threads * tasks_per_thread tasks to equally share load between
/// threads.
[[nodiscard]] static std::vector<std::pair<value_type, value_type>> get_required_ranges(
    const global_state& state) {
  std::vector<std::pair<value_type, value_type>> result;

  // Calculate task size to use for load balancing.
  // We want a minimum of 1, but it should be fairly even between tasks.
  value_type total_tasks = state.num_threads * state.tasks_per_thread;
  if (total_tasks == 0) {
    throw runtime_error("Total tasks cannot be zero");
  }
  value_type range_len = state.report_levels.back() - state.start;
  value_type task_size = ONE + range_len / total_tasks;

  value_type current = state.start;
  for (const auto report_level : state.report_levels) {
    while (current <= report_level) {
      // soft_limit is the end of the current task size
      value_type soft_limit;
      if (std::numeric_limits<value_type>::max() - task_size > current) {
        soft_limit = current + task_size;
      } else {
        soft_limit = std::numeric_limits<value_type>::max();
      }
      // If any report comes before then, it is a hard limit
      value_type hard_limit = report_level + ONE;  // Report levels are inclusive,
                                                   // but limit is exclusive, so add 1
      // Final limit is a clamp of the soft limit between 1 and the hard limit
      auto limit = std::clamp(soft_limit, ONE, hard_limit);
      result.push_back({current, limit});
      current = limit;
    }
  }

  return result;
}

/// @brief Loads the journal entries from the journal_filename specified in the
/// state
[[nodiscard]] static std::vector<journal_entry> load_journal_entries(const global_state& state) {
  auto& filename = state.journal_filename;
  auto required_ranges = get_required_ranges(state);

  std::vector<journal_entry> entries;
  std::ifstream in(filename);
  if (!in) {
    return entries;
  }

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }

    std::stringstream ss(line);
    std::string segment;
    journal_entry entry;
    // There should be the same number of counts as differences
    entry.counts.reserve(state.differences.size());

    // Each line should start with the start value
    if (std::getline(ss, segment, ',')) {
      entry.start = parse_value_type(segment);
    } else {
      // Ignore any incorrect lines
      log_message(
          std::format("WARN: Skipping malformed line in {}: {}", state.journal_filename, line));
      continue;
    }

    // Then the end value
    if (std::getline(ss, segment, ',')) {
      entry.end = parse_value_type(segment);
    } else {
      log_message(
          std::format("WARN: Skipping malformed line in {}: {}", state.journal_filename, line));
      continue;
    }

    // Ignore any ranges that are not required
    std::pair range = {entry.start, entry.end};
    if (!std::binary_search(required_ranges.begin(), required_ranges.end(), range)) {
      log_message(
          std::format("WARN: Skipping malformed line in {}: {}", state.journal_filename, line));
      continue;
    }

    // Then the CPU time
    if (std::getline(ss, segment, ',')) {
      entry.cpu_time = std::stod(segment);
    } else {
      log_message(
          std::format("WARN: Skipping malformed line in {}: {}", state.journal_filename, line));
      continue;
    }

    // The number of counts must match state.differences.size()
    // but the vector will just grow as we parse.
    while (std::getline(ss, segment, ',')) {
      entry.counts.push_back(parse_value_type(segment));
    }

    if (state.differences.size() != entry.counts.size()) {
      log_message(
          std::format("WARN: Skipping malformed line in {}: {}", state.journal_filename, line));
      continue;
    }

    entries.push_back(entry);
  }

  // Sort entries by start
  std::ranges::sort(
      entries, [](const journal_entry& a, const journal_entry& b) { return a.start < b.start; });

  return entries;
}

/// @brief Get details of all completed ranges
[[nodiscard]] static std::vector<std::pair<value_type, value_type>> get_completed_ranges(
    const global_state& state) {
  std::vector<std::pair<value_type, value_type>> result;
  auto entries = load_journal_entries(state);
  result.reserve(entries.size());
  for (const auto& [start, end, cpu_time, counts] : entries) {
    result.push_back({start, end});
  }
  return result;
}

[[nodiscard]] std::string get_report_string(const global_state& state) {
  std::string result = "x,h,pi_h(x)\n";
  auto required_ranges = get_required_ranges(state);
  auto entries = load_journal_entries(state);

  if (entries.size() != required_ranges.size()) {
    throw runtime_error(
        std::format("Incorrect number of matching entries in {}", state.journal_filename));
  }

  vector_type running_counts;
  // Include a resize to avoid a false positive warning on gcc 14
  running_counts.resize(state.differences.size(), 0);

  double cpu_time = 0;
  size_t report_index = 0;
  for (size_t i = 0; i < entries.size(); i++) {
    auto& entry = entries[i];
    for (size_t j = 0; j < state.differences.size(); j++) {
      running_counts[j] += entry.counts[j];
    }
    cpu_time += entry.cpu_time;
    // Since report levels are inclusive and end is exclusive,
    // the level to report is 1 less than the end
    auto level = entry.end - 1;
    if (level == state.report_levels[report_index]) {
      for (size_t j = 0; j < state.differences.size(); j++) {
        result += std::format("{},{},{}\n", level, state.differences[j], running_counts[j]);
      }
      report_index++;
      if (report_index >= state.report_levels.size()) {
        break;
      }
    }
  }
  result += std::format("Total CPU time:,{}\n", cpu_time);

  return result;
}

/// @brief Save worker_state to its specified file.
/// Writes to a .tmp file first, then moves it to help avoid corruption during
/// saving.
void save_worker_state(const worker_state& state) {
  std::string tmp_filename = state.worker_filename + ".tmp";

  // Atomic operation on POSIX filesystems
  try {
    if (output_to_file(tmp_filename, worker_state_to_string(state))) {
      std::filesystem::rename(tmp_filename, state.worker_filename);
    }
  } catch (const std::filesystem::filesystem_error& e) {
    // Failure is annoying, but not fatal to calculations, so just output a
    // warning.
    log_message(
        std::format("WARN: Unable to write to file {}: {}", state.worker_filename, e.what()));
  }
}

// Gets the filename for the worker.
// base_{start}-{end}.work
// (where start and end are zero filled to be as long as limit).
[[nodiscard]] static std::string get_filename(const std::string& base, const value_type start,
                                              const value_type end, value_type limit) {
  // Pad filenames with zeros to keep filesystem listing sorted numerically
  int digits = 0;
  while (limit > 0) {
    limit /= 10;
    digits++;
  }
  digits = std::max(1, digits);

  return std::format("{0}_{1:0{3}}-{2:0{3}}.work", base, start, end, digits);
}

/// @brief Generates the list of worker_states to be calculated.
/// Skips any ranges that have already been completed, and attempts to resume
/// any that have been partially calculated.
[[nodiscard]] std::vector<worker_state> get_worker_states(const global_state& state) {
  std::vector<worker_state> result;
  // Initialise counts for each worker to 0
  vector_type zero_count(state.differences.size(), 0);

  auto completed = get_completed_ranges(state);
  auto ranges = get_required_ranges(state);

  for (const auto& range : ranges) {
    // Create a worker_state for each range that hasn't been completed
    if (!std::binary_search(completed.begin(), completed.end(), range)) {
      worker_state worker;
      worker.journal_filename = state.journal_filename;
      worker.start = range.first;
      worker.end = range.second;
      worker.cpu_time = 0;
      worker.current_position = worker.start;
      worker.differences = state.differences;
      worker.checkpoint_frequency = state.checkpoint_frequency;
      worker.parameters = state.parameters;
      worker.worker_filename = get_filename(worker.journal_filename, worker.start, worker.end,
                                            state.report_levels.back());
      worker.current_counts = zero_count;
      // Attempt to load any partially complete data
      try {
        auto existing_worker = parse_worker_state_from_file(worker);
        worker = existing_worker;
      } catch (std::runtime_error& e) {
        log_message(
            std::format("WARN: Unable to load partially completed job "
                        "from {}. Entire range will be recalculated.",
                        worker.worker_filename));
      }

      result.push_back(worker);
    }
  }
  return result;
}
