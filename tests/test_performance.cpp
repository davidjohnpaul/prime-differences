// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include <atomic>
#include <print>

#include "doctest.h"
#include "types.hpp"
#include "worker_impl.hpp"

// Tests that check/measure performance of the system.

TEST_CASE("Performance: Large Range" * doctest::skip()) {
  // Manually run test with --no-skip
  KEEP_RUNNING.test_and_set();
  worker_state state;
  state.start = 0;
  state.end = 1e9;
  state.differences = {0, 2};
  state.current_position = state.start;
  state.current_counts = {0, 0};
  state.cpu_time = 0;
  state.checkpoint_frequency = 0;

  auto start = std::chrono::high_resolution_clock::now();
  Worker worker(state);
  worker.start();
  auto end = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::println("Processed up to {} in {}ms", state.end, duration.count());

  // Verify correctness
  CHECK(state.current_counts[0] == 50847534);  // Known prime count
  CHECK(state.current_counts[1] == 3424506);   // Known twin prime count
}
