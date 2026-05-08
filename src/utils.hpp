// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#pragma once
#include <source_location>
#include <stdexcept>

#include "types.hpp"

// Utility functions to perform some basic operations.

/// @brief Removes leading and trailing whitespace from a string in-place.
///
/// Essential for parsing configuration lines cleanly, especially when handling
/// cross-platform line endings (\r\n vs \n) or inadvertent spaces added by
/// users.
void trim(std::string& s);

/// @brief Thread-safe logging wrapper that specifies where the message came
/// from.
void log_message(const std::string& msg,
                 const std::source_location& loc = std::source_location::current());

/// @brief Ensure directory for the given file exists.
void ensure_directory_exists(const std::string& filename);

/// @brief Outputs the given content to the given file (overwriting anything
/// that already exists), returning true if successful.
bool output_to_file(const std::string& filename, const std::string& contents);

/// @brief Some quick sanity checks on a worker_state to ensure it is valid.
void check_worker_state_is_valid(const worker_state& state);

/// @brief A runtime_error that reports the location where the error occurred.
class runtime_error : public std::runtime_error {
 public:
  runtime_error(const std::string& msg, std::source_location loc = std::source_location::current())
      : std::runtime_error(format_message(msg, loc)) {}

 private:
  /// @brief A string formatter to add details of file name, line, and name of
  /// function where the formatter was called.
  static std::string format_message(const std::string& msg, const std::source_location& loc) {
    return std::format("[{}:{} in {}] {}", loc.file_name(), loc.line(), loc.function_name(), msg);
  }
};
