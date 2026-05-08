// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "utils.hpp"

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <stdexcept>
#include <system_error>

/// @brief Removes leading and trailing whitespace from a string in-place.
void trim(std::string& s) {
  if (!s.empty()) {
    size_t i = 0;
    // Trim Left: Advance index past whitespace
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
      i++;
    }
    s.erase(0, i);
    // Trim Right: Decrement index past whitespace
    i = s.size();
    while (i > 0 && std::isspace(static_cast<unsigned char>(s[i - 1]))) {
      i--;
    }
    s.erase(i);
  }
}

// Outputs time, where the log message was requested, and the message to stderr.
void log_message(const std::string& msg, const std::source_location& loc) {
  auto now = std::chrono::system_clock::now();
  std::println(stderr, "{:%c}: [{}: {} in {}] {}", now, loc.file_name(), loc.line(),
               loc.function_name(), std::vformat("{}", std::make_format_args(msg)));
}

/// @brief Ensure directory for the given file exists.
void ensure_directory_exists(const std::string& filename) {
  std::filesystem::path path(filename);

  if (path.has_parent_path()) {
    std::error_code code;
    std::filesystem::create_directories(path.parent_path(), code);
    if (code) {
      throw runtime_error(
          std::format("Failed to create directory for {}: {}", filename, code.message()));
    }
  }
}

/// @brief Outputs the given content to the given file (overwriting anything
/// that already exists).
bool output_to_file(const std::string& filename, const std::string& contents) {
  std::ofstream out(filename);
  if (!out) {
    throw runtime_error(std::format("Failed to open {} for writing", filename));
  }
  out << contents;
  if (!out) {
    throw runtime_error(std::format("Failed to write to {}", filename));
  }
  return true;
}

/// @brief Some quick sanity checks on a worker_state.
/// Ensures:
/// There is at least one difference to count.
/// Each difference to count is even.
/// The size of differences and current_counts are identical.
/// start is less than end
/// current_position is in [start, end]
/// checkpoint_frequency is non-negative
/// cpu_time is non-negative
void check_worker_state_is_valid(const worker_state& state) {
  if (state.differences.empty()) {
    throw runtime_error(
        std::format("differences must contain at least one value in {}", state.worker_filename));
  }

  for (auto difference : state.differences) {
    if (difference % 2 == 1) {
      throw runtime_error(std::format("difference {} is not divisible by 2 in {}", difference,
                                      state.worker_filename));
    }
  }

  if (state.differences.size() != state.current_counts.size()) {
    throw runtime_error(std::format("current_counts must have same size as differences in {}",
                                    state.worker_filename));
  }

  if (state.end <= state.start) {
    throw runtime_error(std::format("end must be greater than start in {}", state.worker_filename));
  }

  if (state.current_position < state.start || state.current_position > state.end) {
    throw runtime_error(
        std::format("current_position must be between start and end in {}", state.worker_filename));
  }

  if (state.checkpoint_frequency < 0) {
    throw runtime_error(
        std::format("checkpoint_frequency must be non-negative in {}", state.worker_filename));
  }

  if (state.cpu_time < 0) {
    throw runtime_error(std::format("cpu_time must be non-negative in {}", state.worker_filename));
  }
}
