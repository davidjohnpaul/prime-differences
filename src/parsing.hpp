// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#pragma once
#include "types.hpp"

// Functions to parse from and convert to strings

/// @brief Converts a string entry to the project's native integer type.
/// Wraps std::stoull with error handling.
[[nodiscard]] value_type parse_value_type(const std::string& str);

/// @brief Parses a Comma-Separated Value (CSV) string into a vector.
///
/// Example Input: "2,4,6,8"
[[nodiscard]] vector_type parse_vector_type(const std::string& str);

/// @brief Parses Comma-Separated Value (CSV) strings from a file into a single
/// vector.
///
/// Reads each non-empty line that doesn't start with a # from the file, parses
/// the values from it using parse_vector_type, and combines it all into a
/// single vector.
[[nodiscard]] vector_type parse_vector_type_from_file(const std::string& filename);

/// @brief Attempts to load a worker_state from the file with the given
/// filename.
[[nodiscard]] worker_state parse_worker_state_from_file(const std::string& filename);

/// @brief Attempts to load the current state of the given worker_state from the
/// worker_filename it specifies and verifies that it matches base_state.
///
/// If no such file exists, the base_state is returned as-is.
[[nodiscard]] worker_state parse_worker_state_from_file(const worker_state& base_state);

/// @brief Converts a vector to a standard CSV string.
///
/// format: "1,2,3"
[[nodiscard]] std::string vector_type_to_string(const vector_type& vec);

/// @brief Converts a worker_state to a string using a key:value format.
[[nodiscard]] std::string worker_state_to_string(const worker_state& state);
