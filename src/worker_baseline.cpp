// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "worker_baseline.hpp"

#include <atomic>
#include <primesieve.hpp>

#include "types.hpp"

Worker_baseline::Worker_baseline(worker_state& state) : WorkerBase(state) {
  auto max_gap = state_.differences.back();

  // Gaps have to be even, so we can ignore odd gaps.
  counts_.resize(max_gap / 2 + 1, 0);

  targets_.resize(max_gap / 2 + 1, false);
  for (auto g : state_.differences) {
    targets_[g / 2] = true;
  }
}

bool Worker_baseline::handle_checkpoint(bool force) {
  if (force || output_progress()) {
    // Move our internal counts to the state_
    for (size_t i = 0; i < state_.differences.size(); i++) {
      auto diff = state_.differences[i];
      state_.current_counts[i] += counts_[diff / 2];
      counts_[diff / 2] = 0;
    }
    state_.current_position = std::min(state_.end, prime_ - state_.differences.back() + ONE);
    return true;
  }
  return false;
}

void Worker_baseline::run() {
  if (state_.current_position >= state_.end) {
    return;  // Already complete
  }

  auto start = state_.current_position;
  auto end = state_.end;
  auto max_gap = state_.differences.back();

  primesieve::iterator it;
  it.jump_to(start);
  prime_ = it.next_prime();

  // Handle the only even prime
  if (prime_ == 2) {
    if (state_.differences[0] == 0 && state_.end > prime_) {
      state_.current_counts[0]++;
    }
    state_.current_position = std::min(state_.end, prime_ + ONE);
    prime_ = it.next_prime();
  }

  while (prime_ < end + max_gap) {
    // Check if we should stop
    if (!KEEP_RUNNING.test(std::memory_order_relaxed)) {
      break;
    }

    // Add the prime if we may need to count it
    if (prime_ < end) {
      history_.push_back(prime_);
    }
    // Remove any primes from history where the gap with the current prime is
    // too large
    while (!history_.empty() && (prime_ - history_.front()) > max_gap) {
      history_.pop_front();
    }

    // Check current prime with all remaining history to see if there's a gap we
    // need to count
    for (value_type prev_prime : history_) {
      value_type diff = prime_ - prev_prime;
      if (diff <= max_gap && targets_[diff / 2]) {
        counts_[diff / 2]++;
      }
    }

    // Checkpoint and move on
    handle_checkpoint();
    prime_ = it.next_prime();
  }
  handle_checkpoint(true);
}
