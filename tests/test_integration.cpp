// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include <filesystem>

#include "doctest.h"
#include "parsing.hpp"
#include "state.hpp"

// Tests that check integration points.
// e.g., working with the file system

TEST_CASE("File I/O: Save and Load Worker State") {
  worker_state state;
  state.journal_filename = "";
  state.worker_filename = "test_worker_temp.work";
  state.start = 100;
  state.end = 1000;
  state.current_position = 500;
  state.differences = {2, 4, 6};
  state.current_counts = {10, 20, 30};
  state.cpu_time = 123.45;
  state.checkpoint_frequency = 60;
  state.parameters = "TESTING";

  // Save
  save_worker_state(state);

  // Load
  worker_state base;
  base.journal_filename = state.journal_filename;
  base.worker_filename = state.worker_filename;
  base.start = state.start;
  base.end = state.end;
  base.differences = state.differences;
  base.checkpoint_frequency = state.checkpoint_frequency;
  base.parameters = state.parameters;
  // Give state a different current_position and current_counts
  base.current_position = base.start;
  base.current_counts = {0, 0, 0};

  worker_state loaded = parse_worker_state_from_file(base);

  CHECK(loaded.start == state.start);
  CHECK(loaded.end == state.end);
  CHECK(loaded.current_position == state.current_position);
  CHECK(loaded.differences == state.differences);
  CHECK(loaded.current_counts == state.current_counts);
  CHECK(loaded.cpu_time == state.cpu_time);
  CHECK(loaded.parameters == state.parameters);

  // Cleanup
  std::filesystem::remove(state.worker_filename);
  std::filesystem::remove(state.worker_filename + ".tmp");
}
