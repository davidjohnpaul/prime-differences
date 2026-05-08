// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#pragma once
#include "types.hpp"

// Allow selection of different Worker implementations using the
// SELECTED_VARIANT value.

// Forward declare all worker implementations.
class Worker_optimized;
class Worker_experimental;
class Worker_baseline;

// Set a default variant.
#ifndef SELECTED_VARIANT
#define SELECTED_VARIANT 0
#endif

// Set Worker to be the variant that has been selected.
#if SELECTED_VARIANT == 0
#define SELECTED_VARIANT_NAME "Optimized"
using Worker = Worker_optimized;
#elif SELECTED_VARIANT == 1
#define SELECTED_VARIANT_NAME "Experimental"
using Worker = Worker_experimental;
#elif SELECTED_VARIANT == 2
#define SELECTED_VARIANT_NAME "Baseline"
using Worker = Worker_baseline;
#else
#error "Unknown SELECTED_VARIANT"
#endif

/// @brief A worker that processes a worker_state.
///
/// The WorkerBase:
/// Ensures the given worker_state is valid.
/// Stores the worker_state that is to be processed, allowing access to the
/// particular variant selected. Tracks the time it takes for the implementation
/// to perform the calculations. Checkpoints the current state when enough time
/// has passed and it is requested to do so. Outputs the compelted state
/// information to the required journal file.
class WorkerBase {
 public:
  /// @brief Stores a timer, calls run on the worker, and checkpoints the
  /// current worker_state, writeing to the journal file if completed.
  void start();

  /// @brief If time for a checkpoint, then save the
  /// current worker_state to disk and output its current progress.
  /// @returns true when progress is output, false otherwise.
  bool output_progress();

  // Gets a description of the parameters supported by this worker.
  static std::string get_parameter_description();

 protected:
  // The state to be processed.
  worker_state& state_;

  /// @brief Validates the given state and stores for future processing.
  /// @param state The mutable state object. The worker updates
  /// state.current_position and state.current_counts to track its progress.
  explicit WorkerBase(worker_state& state);
  virtual ~WorkerBase();

  /// @brief Main execution loop.
  virtual void run() = 0;

 private:
  // The start time of the timer. Used to determine when checkpointing should
  // occur and to update the CPU time required by the worker.
  double start_time_;

  /// @brief Checkpoints the current worker_state to disk if enough time has
  /// passed and checkpointing is enabled.
  /// @param force if true, a checkpoint is performed regardless of time that
  /// has passed (provided checkpointing is enabled).
  bool handle_checkpoint(bool force = false);
};

/// @brief Wrapper function to instantiate and start a Worker, handling any
/// errors. Compatible with std::thread constructor.
void run_worker(worker_state& state);
