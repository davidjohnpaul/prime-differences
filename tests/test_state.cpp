// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "doctest.h"
#include "state.hpp"

// Tests that ensure state is handled correctly.

TEST_CASE("get_worker_states basic functionality") {
  global_state state;
  state.journal_filename = "";
  state.start = 0;
  state.differences = {2, 4};
  state.report_levels = {10000};
  state.num_threads = 2;
  state.tasks_per_thread = 2;
  state.checkpoint_frequency = 0;
  state.parameters = "";

  auto workers = get_worker_states(state);

  // Should create multiple chunks for load balancing
  CHECK(workers.size() >= state.num_threads);

  // Verify coverage (all workers together should cover [start, end))
  value_type covered_start = workers.front().start;
  value_type covered_end = workers.back().end;
  CHECK(covered_start == state.start);
  CHECK(covered_end >= state.report_levels.back());

  // Verify no gaps or overlaps
  for (size_t i = 1; i < workers.size(); i++) {
    CHECK(workers[i].start == workers[i - 1].end);
  }
}

TEST_CASE("get_worker_states with multiple report levels") {
  global_state state;
  state.journal_filename = "";
  state.start = 0;
  state.differences = {2};
  state.report_levels = {1000, 10000, 100000};
  state.num_threads = 2;
  state.tasks_per_thread = 1;
  state.checkpoint_frequency = 0;

  auto workers = get_worker_states(state);

  // Workers should be split at report boundaries
  // This ensures we can report intermediate results
  CHECK(workers.size() > 3);  // At least one chunk per report level
  bool found_1000 = false;
  bool found_10000 = false;
  bool found_100000 = false;
  for (size_t i = 0; i < workers.size(); i++) {
    if (workers[i].end == 1001) {
      found_1000 = true;
    } else if (workers[i].end == 10001) {
      found_10000 = true;
    } else if (workers[i].end == 100001) {
      found_100000 = true;
    }
  }
  CHECK(found_1000);
  CHECK(found_10000);
  CHECK(found_100000);
}
