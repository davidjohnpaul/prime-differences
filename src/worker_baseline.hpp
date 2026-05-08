// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include <deque>

#include "types.hpp"
#include "worker.hpp"

/// @brief A worker that generates all primes in the required range, tracking
/// history to allow all future primes to be compared with past values that
/// could give a difference that needs to be counted.
class Worker_baseline : public WorkerBase {
 public:
  explicit Worker_baseline(worker_state& state);

  void run() override final;

  using WorkerBase::get_parameter_description;

 private:
  /// @brief Primes that have been discovered that are still needed to be
  /// compared to later primes.
  std::deque<value_type> history_;
  /// @brief An array of required targets so for any pair of primes p1>=p2, we
  /// can just check targets_[(p1-p2)/2] to see whether it is a difference we
  /// are tracking.
  std::vector<bool> targets_;
  /// @brief The count of each difference that we have not yet output to the
  /// worker_state, such that counts_[i] represents the count of primes with
  /// difference 2i that have been counted.
  vector_type counts_;
  /// @brief The last prime we have considered.
  value_type prime_;

  /// @brief Transers the current count to the state object and outputs it if
  /// required.
  /// @params force specifies that values should be moved to the state object,
  /// whether or not output is required
  bool handle_checkpoint(bool force = false);
};
