// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "doctest.h"
#include "types.hpp"
#include "worker_impl.hpp"

// Tests that ensure data are validated properly.
// i.e., errors are thrown if parameters are incorrect

TEST_CASE("Invalid Workers") {
  KEEP_RUNNING.test_and_set();

  SUBCASE("Empty differences") {
    worker_state state;
    state.start = 3;
    state.end = 1000;
    state.differences = {};
    state.current_position = state.start;
    state.current_counts = {};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;

    CHECK_THROWS_AS([&]() { Worker worker(state); }(), std::runtime_error);
  }

  SUBCASE("Difference not divisible by 2") {
    worker_state state;
    state.start = 3;
    state.end = 1000;
    state.differences = {2, 3};
    state.current_position = state.start;
    state.current_counts = {0, 0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;

    CHECK_THROWS_AS([&]() { Worker worker(state); }(), std::runtime_error);
  }
  SUBCASE("current_count different size from differences") {
    worker_state state;
    state.start = 3;
    state.end = 1000;
    state.differences = {2};
    state.current_position = state.start;
    state.current_counts = {0, 0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;

    CHECK_THROWS_AS([&]() { Worker worker(state); }(), std::runtime_error);
  }

  SUBCASE("End less than start") {
    worker_state state;
    state.start = 3;
    state.end = 1;
    state.differences = {0};
    state.current_position = state.start;
    state.current_counts = {0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;

    CHECK_THROWS_AS([&]() { Worker worker(state); }(), std::runtime_error);
  }

  SUBCASE("current_position before start") {
    worker_state state;
    state.start = 3;
    state.end = 1000;
    state.differences = {0};
    state.current_position = state.start - 1;
    state.current_counts = {0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;

    CHECK_THROWS_AS([&]() { Worker worker(state); }(), std::runtime_error);
  }

  SUBCASE("current_position after end") {
    worker_state state;
    state.start = 3;
    state.end = 1000;
    state.differences = {0};
    state.current_position = state.end + 1;
    state.current_counts = {0};
    state.cpu_time = 0;
    state.checkpoint_frequency = 0;

    CHECK_THROWS_AS([&]() { Worker worker(state); }(), std::runtime_error);
  }

  SUBCASE("cpu_time negative") {
    worker_state state;
    state.start = 3;
    state.end = 1000;
    state.differences = {0};
    state.current_position = state.start;
    state.current_counts = {0};
    state.cpu_time = -1;
    state.checkpoint_frequency = 0;

    CHECK_THROWS_AS([&]() { Worker worker(state); }(), std::runtime_error);
  }

  SUBCASE("checkpoint_frequency negative") {
    worker_state state;
    state.start = 3;
    state.end = 1000;
    state.differences = {0};
    state.current_position = state.start;
    state.current_counts = {0};
    state.cpu_time = 0;
    state.checkpoint_frequency = -1;

    CHECK_THROWS_AS([&]() { Worker worker(state); }(), std::runtime_error);
  }
}
