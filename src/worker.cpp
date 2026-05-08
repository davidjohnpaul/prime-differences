// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "worker.hpp"

#include <ctime>  // Required for std::clock, CLOCKS_PER_SEC
#include <filesystem>
#include <stdexcept>

#include "state.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "worker_impl.hpp"

/// @brief Gets high-precision CPU time consumed by the current thread.
/// Unlike wall-clock time, this doesn't "tick" if the thread is sleeping
/// or preempted by the OS.
[[nodiscard]] static inline double get_thread_cpu_time() {
// Try to use Linux-specific High-Precision Thread Timer
#if defined(CLOCK_THREAD_CPUTIME_ID)
  timespec ts;
  // CLOCK_THREAD_CPUTIME_ID is a Linux-specific extension that allows
  // per-thread accounting.
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
  }
#endif
  // Generic Fallback (Windows, macOS, etc.)
  // std::clock() returns "processor time used by the program".
  // On Windows, this is often accurate enough for basic profiling,
  // though it may aggregate time across threads on some OSs.
  return static_cast<double>(std::clock()) / static_cast<double>(CLOCKS_PER_SEC);
}

WorkerBase::WorkerBase(worker_state& state) : state_(state) {
  // Ensure state passes some necessary checks.
  check_worker_state_is_valid(state_);
}

WorkerBase::~WorkerBase() = default;

void WorkerBase::start() {
  if (!KEEP_RUNNING.test(std::memory_order_relaxed)) {
    return;
  }

  // Initialise timer.
  start_time_ = get_thread_cpu_time();

  // Start the worker.
  run();

  // Finalise
  handle_checkpoint(true);
  // Add to journal if all done.
  if (state_.current_position >= state_.end) {
    append_completion_record(state_);
    if (std::filesystem::exists(state_.worker_filename)) {
      std::filesystem::remove(state_.worker_filename);
    }
    if (std::filesystem::exists(state_.worker_filename + ".tmp")) {
      std::filesystem::remove(state_.worker_filename + ".tmp");
    }
  }
}

bool WorkerBase::handle_checkpoint(bool force) {
  bool result = false;
  double end_time = get_thread_cpu_time();
  // Check if checkpoint interval has elapsed or we need to update.
  if (force || (state_.checkpoint_frequency > 0 &&
                (end_time - start_time_) >= state_.checkpoint_frequency)) {
    // Updates state structs
    state_.cpu_time += (end_time - start_time_);
    start_time_ = end_time;  // Reset timer

    // Only save to disk if we are checkpointing.
    if (state_.checkpoint_frequency > 0) {
      save_worker_state(state_);
      result = true;
    }
  }
  return result;
}

bool WorkerBase::output_progress() {
  if (handle_checkpoint()) {
    // Calculcate percentage complete.
    double percent = 100.0 * static_cast<double>(state_.current_position - state_.start) /
                     static_cast<double>(state_.end - state_.start);

    // Log to console.
    log_message(std::format("INFO: [Worker {}-{}]: current position: {} ({}%) in {:.2f}s",
                            state_.start, state_.end, state_.current_position, percent,
                            state_.cpu_time));
    return true;
  }
  return false;
}

std::string WorkerBase::get_parameter_description() {
  return "are ignored. No parameters supported.";
}

void run_worker(worker_state& state) {
  if (state.current_position < state.end) {
    try {
      Worker worker(state);
      worker.start();
    } catch (const std::runtime_error& e) {
      fprintf(stderr, "Fatal Error in worker: %s\n", e.what());
    }
  }
}
