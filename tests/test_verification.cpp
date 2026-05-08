// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include <atomic>
#include <numeric>

#include "doctest.h"
#include "types.hpp"
#include "worker_impl.hpp"

// Tests that verify the algorithm is working correctly.

TEST_CASE("Small Range Verification") {
  // Setup: We want to count differences in range [3, 100,000]
  // Known Primes count: 9592
  // Known Twin Primes (diff 2) count: 1224
  // Known Cousin Primes (diff 4) count: 1216
  // Known Sexy Primes (diff 6) count: 2447
  KEEP_RUNNING.test_and_set();

  SUBCASE("Zero start") {
    worker_state state;
    state.journal_filename = "";
    state.worker_filename = "";
    state.start = 0;
    state.end = 100000;
    state.differences = {0, 2, 4, 6};
    state.current_position = state.start;
    state.current_counts = {0, 0, 0, 0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;

    Worker worker(state);
    worker.start();

    CHECK(state.current_counts[0] == 9592);
    CHECK(state.current_counts[1] == 1224);
    CHECK(state.current_counts[2] == 1216);
    CHECK(state.current_counts[3] == 2447);
  }

  SUBCASE("Non-zero start") {
    worker_state state;
    state.journal_filename = "";
    state.worker_filename = "";
    state.start = 4;
    state.end = 100000;
    state.differences = {0, 2, 4, 6};
    state.current_position = state.start;
    state.current_counts = {0, 0, 0, 0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;

    Worker worker2(state);
    worker2.start();
    CHECK(state.current_counts[0] == 9590);
    CHECK(state.current_counts[1] == 1223);
    CHECK(state.current_counts[2] == 1215);
    CHECK(state.current_counts[3] == 2447);
  }
}

TEST_CASE("Range Smaller Than Segment") {
  KEEP_RUNNING.test_and_set();
  worker_state state;
  state.start = 100;
  state.end = 150;  // Only 50 numbers
  state.differences = {0};
  state.current_position = state.start;
  state.current_counts = {0};
  state.cpu_time = 0;
  state.checkpoint_frequency = 0;

  Worker worker(state);
  worker.start();

  // Should still work correctly
  // Primes in [100, 150): 101, 103, 107, 109, 113, 127, 131, 137, 139, 149 = 10
  CHECK(state.current_counts[0] == 10);
}

TEST_CASE("Non-Aligned Start") {
  KEEP_RUNNING.test_and_set();
  worker_state state;
  state.journal_filename = "";
  state.worker_filename = "";
  state.start = 101;  // Odd start
  state.end = 200;
  state.differences = {0};
  state.current_position = state.start;
  state.current_counts = {0};
  state.cpu_time = 0;
  state.checkpoint_frequency = 0;

  Worker worker(state);
  worker.start();

  // Primes in [100, 200): 21 primes
  CHECK(state.current_counts[0] == 21);
}

TEST_CASE("Non-Aligned End") {
  KEEP_RUNNING.test_and_set();
  worker_state state;
  state.start = 3;
  state.end = 127;  // Odd end
  state.differences = {0};
  state.current_position = state.start;
  state.current_counts = {0};
  state.cpu_time = 0;
  state.checkpoint_frequency = 0;

  Worker worker(state);
  worker.start();

  // Primes in [3, 127): 30 primes
  CHECK(state.current_counts[0] == 29);
}

TEST_CASE("Resume Produces Same Result") {
  KEEP_RUNNING.test_and_set();

  // Fresh run
  worker_state state1;
  state1.journal_filename = "";
  state1.worker_filename = "";
  state1.start = 0;
  state1.end = 10000;
  state1.differences = {2, 6};
  state1.current_position = state1.start;
  state1.current_counts = {0, 0};
  state1.cpu_time = 0;
  state1.checkpoint_frequency = 0;

  Worker worker1(state1);
  worker1.start();

  // Interrupted run (simulate checkpoint at position 5000)
  worker_state state2 = state1;
  state2.current_position = state2.start;
  state2.current_counts = {0, 0};
  // Manually stop early (simulating checkpoint)
  state2.end = std::midpoint(state1.start, state1.end);

  Worker worker2(state2);
  worker2.start();

  // Resume from checkpoint
  worker_state state3 = state2;
  state3.end = state1.end;
  Worker worker3(state3);
  worker3.start();

  // Results should match
  CHECK(state3.current_counts[0] == state1.current_counts[0]);
  CHECK(state3.current_counts[1] == state1.current_counts[1]);
}

TEST_CASE("Very Small Ranges") {
  KEEP_RUNNING.test_and_set();

  SUBCASE("Zero count") {
    worker_state state;
    state.start = 0;
    state.end = 2;
    state.differences = {0, 2};
    state.current_position = state.start;
    state.current_counts = {0, 0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;

    Worker worker(state);
    worker.start();

    CHECK(state.current_counts[0] == 0);
    CHECK(state.current_counts[1] == 0);
    CHECK(state.current_position == state.end);
  }

  SUBCASE("Small Count") {
    worker_state state;
    state.start = 3;
    state.end = 5;
    state.differences = {0, 2, 4, 6};
    state.current_position = state.start;
    state.current_counts = {0, 0, 0, 0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;

    Worker worker(state);
    worker.start();

    CHECK(state.current_counts[0] == 1);
    CHECK(state.current_counts[1] == 1);
    CHECK(state.current_counts[2] == 1);
    CHECK(state.current_counts[3] == 0);
  }
}
