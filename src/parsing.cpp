// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "parsing.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "utils.hpp"

/// @brief Converts a string to a value_type.
///
/// Wraps std::stoull, raising errors if the string is empy, starts with a
/// negative sign, or has unexpected trailing characters.
[[nodiscard]] value_type parse_value_type(const std::string& str) {
  auto pos = str.find_first_not_of(" \t\n\r\f\v");
  // Ensure there is at least one non-whitespace character.
  if (pos == std::string::npos) {
    throw runtime_error("Cannot convert empty string to value_type");
  }
  // Ensure the first non-whitespace character is not -.
  if (str[pos] == '-') {
    throw runtime_error("Cannot convert a negative value to value_type");
  }

  try {
    pos = 0;
    unsigned long long val = std::stoull(str, &pos, 10);

    // Ensure entire string was used.
    if (pos != str.size()) {
      throw runtime_error(
          std::format("Cannot convert an invalid unsigned integer ({}) to value_type", str));
    }
    if (val > std::numeric_limits<value_type>::max()) {
      throw runtime_error(std::format("Value {} exceeds maximum type value of {}", str,
                                      std::numeric_limits<value_type>::max()));
    }
    return static_cast<value_type>(val);
  } catch (const std::invalid_argument& e) {
    throw runtime_error(
        std::format("Cannot convert an invalid unsigned integer to value_type: {}", e.what()));
  } catch (const std::out_of_range& e) {
    throw runtime_error(
        std::format("Value out of range for converstion to value_type: {}", e.what()));
  }
}

/// @brief Parses a comma-separated list (CSV) of value_types.
[[nodiscard]] vector_type parse_vector_type(const std::string& str) {
  vector_type result;
  std::istringstream iss(str);
  std::string component;
  while (std::getline(iss, component, ',')) {
    result.push_back(parse_value_type(component));
  }
  return result;
}

/// @brief Parses all values from the given file.
[[nodiscard]] vector_type parse_vector_type_from_file(const std::string& filename) {
  vector_type result;

  std::ifstream in(filename);
  if (!in) {
    throw runtime_error(std::format("Failed to open {} for reading", filename));
  }
  std::string line;
  while (std::getline(in, line)) {
    trim(line);
    // Ignore empty lines and comments
    if (!line.empty() && !line.starts_with('#')) {
      vector_type line_values = parse_vector_type(line);
      result.insert(result.end(), line_values.begin(), line_values.end());
    }
  }
  if (in.bad()) {
    throw runtime_error(std::format("I/O error while reading {}", filename));
  }

  return result;
}

[[nodiscard]] worker_state parse_worker_state_from_file(const std::string& filename) {
  // If file doesn't exist, raise an error
  if (!std::filesystem::exists(filename)) {
    throw runtime_error(std::format("{} does not exist!", filename));
  }

  //  Load state from file
  worker_state result;
  result.journal_filename = "";
  result.worker_filename = filename;
  result.differences = {};
  result.current_counts = {};
  result.start = 0;
  result.end = 0;
  result.current_position = 0;
  result.checkpoint_frequency = 0;
  result.cpu_time = 0;
  result.parameters = "";

  std::ifstream in(filename);
  if (!in) {
    throw runtime_error(std::format("Failed to open {} for reading", filename));
  }
  std::string line;
  while (std::getline(in, line)) {
    //  Split Key/Value
    auto pos = line.find(":");
    std::string left;
    std::string right;
    if (pos == std::string::npos) {
      left = line;
      right = "";
    } else {
      left = line.substr(0, pos);
      right = line.substr(pos + 1);
    }
    trim(left);
    trim(right);

    // Map keys to struct fileds
    if (left == "differences") {
      result.differences = parse_vector_type(right);
    } else if (left == "current_counts") {
      result.current_counts = parse_vector_type(right);
    } else if (left == "start") {
      result.start = parse_value_type(right);
    } else if (left == "end") {
      result.end = parse_value_type(right);
    } else if (left == "current_position") {
      result.current_position = parse_value_type(right);
    } else if (left == "checkpoint_frequency") {
      result.checkpoint_frequency = std::stod(right);
    } else if (left == "cpu_time") {
      result.cpu_time = std::stod(right);
    } else if (left == "parameters") {
      result.parameters = right;
    } else if (left.empty() || left.starts_with('#')) {
      // Ignore empty lines and comments
    } else {
      throw runtime_error(std::format("Unknown option ({}) in {}", left, filename));
    }
  }

  if (in.bad()) {
    throw runtime_error(std::format("I/O error while reading {}", filename));
  }

  // Ensure result state is valid
  check_worker_state_is_valid(result);

  return result;
}

[[nodiscard]] worker_state parse_worker_state_from_file(const worker_state& base_state) {
  // If file doesn't exist, just return the base state
  if (!std::filesystem::exists(base_state.worker_filename)) {
    return base_state;
  }

  //  Load state from file
  worker_state result = parse_worker_state_from_file(base_state.worker_filename);

  result.journal_filename = base_state.journal_filename;

  // Ensure result state is valid

  if (result.start != base_state.start) {
    throw runtime_error(std::format("Mis-matched start in ", result.worker_filename));
  }

  if (result.end != base_state.end) {
    throw runtime_error(std::format("Mis-matched end in ", result.worker_filename));
  }

  if (result.differences != base_state.differences) {
    throw runtime_error(std::format("Mis-matched differences in ", result.worker_filename));
  }

  return result;
}

/// @brief Convert a vector_type to a CSV String.
[[nodiscard]] std::string vector_type_to_string(const vector_type& vec) {
  if (vec.empty()) {
    return {};
  }

  std::string result;
  // Estimate 10 chars per number + comma to reduce reallocs
  result.reserve(vec.size() * 10);

  // Add first element, then prepend commas to subsequent ones
  result += std::to_string(vec[0]);
  for (size_t i = 1; i < vec.size(); i++) {
    result += "," + std::to_string(vec[i]);
  }

  return result;
}

/// @brief worker_state to String.
[[nodiscard]] std::string worker_state_to_string(const worker_state& state) {
  std::ostringstream out;
  //  out << "# start is the value to start searching from\n";
  out << std::format("start: {}\n", state.start);
  //  out << "# end is the value to search up until (not included)\n";
  out << std::format("end: {}\n", state.end);
  //  out << "# differences is the set of differences to calculate counts
  //  for\n";
  out << std::format("differences: {}\n", vector_type_to_string(state.differences));
  //  out << "# current_position is where the system has currently counted up to
  //  "
  //         "(exclusive)\n";
  out << std::format("current_position: {}\n", state.current_position);
  //  out << "# current_counts saves the count up to current_position\n";
  out << std::format("current_counts: {}\n", vector_type_to_string(state.current_counts));
  //  out << "#cpu_time records the amount of time this worker has spent "
  //         "processing\n";
  out << std::format("cpu_time: {}\n", state.cpu_time);
  //  out << "#parameters records parameters to be passed along to the worker
  //  implementation\n";
  out << std::format("parameters: {}\n", state.parameters);
  return out.str();
}
