// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "doctest.h"
#include "types.hpp"
#include "worker.hpp"
#include "worker_optimized.hpp"

// Tests that ensure data are validated properly.
// i.e., errors are thrown if parameters are incorrect

TEST_CASE("Segment size is valid") {
  KEEP_RUNNING.test_and_set();

  SUBCASE("Largest difference longer than segment") {
    worker_state state;
    state.start = 3;
    state.end = 1000;
    state.differences = {1000000};
    state.current_position = state.start;
    state.current_counts = {0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;
    state.parameters = "128";

    CHECK_THROWS_AS([&]() { Worker_optimized worker(state); }(), std::runtime_error);
  }

  SUBCASE("Segment size not a power of 2") {
    worker_state state;
    state.start = 3;
    state.end = 1000;
    state.differences = {2, 4};
    state.current_position = state.start;
    state.current_counts = {0, 0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;
    state.parameters = "129";

    CHECK_THROWS_AS([&]() { Worker_optimized worker(state); }(), std::runtime_error);
  }

  SUBCASE("Segment size too small") {
    worker_state state;
    state.start = 3;
    state.end = 1000;
    state.differences = {2, 4};
    state.current_position = state.start;
    state.current_counts = {0, 0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;
    state.parameters = std::format("{}", VALUES_PER_WORD / 2);
    CHECK_THROWS_AS([&]() { Worker_optimized worker(state); }(), std::runtime_error);
  }

  SUBCASE("Default segment size is reasonable") {
    worker_state state;
    state.start = 0;
    state.end = 100;
    state.differences = {0, 2};
    state.current_position = state.start;
    state.current_counts = {0, 0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;
    state.parameters = "";

    run_worker(state);
    CHECK(state.current_counts[0] == 25);
    CHECK(state.current_counts[1] == 8);
  }
}
