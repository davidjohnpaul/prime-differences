// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#pragma once
#include <string>

#include "types.hpp"

// Functions to store and report on state.

/// @brief Breaks up the work required for state into a bunch of individual
/// worker_states, splitting the range to allow more even use of threads.
[[nodiscard]] std::vector<worker_state> get_worker_states(const global_state& state);

/// @brief Writes the given worker_state to disk, using the filename in its
/// worker_filename field.
void save_worker_state(const worker_state& state);

/// @brief Appends details from a completed worker_state to the main journal
/// file.
void append_completion_record(const worker_state& state);

/// @brief Gets a string where each line represents the counts for each of the
/// required differences for each of the required report levels in the given
/// state.
[[nodiscard]] std::string get_report_string(const global_state& state);
