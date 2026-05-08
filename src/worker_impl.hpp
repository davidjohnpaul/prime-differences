// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "worker.hpp"

// Includes the header for the chosen worker variant.

#ifndef SELECTED_VARIANT
#define SELECTED_VARIANT 0
#endif

#if SELECTED_VARIANT == 0
#include "worker_optimized.hpp"
using Worker = Worker_optimized;
#elif SELECTED_VARIANT == 1
#include "worker_experimental.hpp"
using Worker = Worker_experimental;
#elif SELECTED_VARIANT == 2
#include "worker_baseline.hpp"
using Worker = Worker_baseline;
#else
#error "Unknown SELECTED_VARIANT"
#endif
