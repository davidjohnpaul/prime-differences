// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#pragma once
#include "types.hpp"

// Functions to get system information.

/// @brief Container for CPU cache hierarchy information (in bytes).
struct cache_sizes {
  value_type l1;  // Level 1 Data Cache (Fastest)
  value_type l2;  // Level 2 Unified/Data Cache
  value_type l3;  // Level 3 Shared Cache
};

// Conservative default values to use if cache detection fails
static constexpr value_type DEFAULT_L1_CACHE_SIZE = 32 * 1024;        // 32KB
static constexpr value_type DEFAULT_L2_CACHE_SIZE = 512 * 1024;       // 512KB
static constexpr value_type DEFAULT_L3_CACHE_SIZE = 8 * 1024 * 1024;  // 8MB

/// @brief Determines CPU cache hierarcy information for the current system.
/// Uses conservative defaults if detection fails.
[[nodiscard]] cache_sizes get_cache_sizes();

/// @brief Estimates the number of registers available on the current system.
[[nodiscard]] value_type get_register_count_heuristic();
