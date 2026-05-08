// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include <primesieve/iterator.hpp>

#include "types.hpp"
#include "worker.hpp"

// The number of values represented by a single word.
// Since we only consider odd values, this is twice the number of bits.
constexpr value_type VALUES_PER_WORD = BITS_PER_WORD * 2;

// The maximum number of differences to consider in one pass through a segment.
// We want this relatively small to encourage the compiler to put them into
// registers.
constexpr size_t MAX_DIFF_TILE_SIZE = 128;

/// @brief The number of words, and bits into the next word, that we need to
/// shift from the current position to determine whether a bit represents a
/// prime pair.
///
/// To check if a difference g matches, we look at bit i and bit i+g. Since i+g
/// is probably part way through a difference word, we pre-calculate the shifts
/// required to compare them.
struct difference_info {
  value_type word_shift;  // How many full words ahead to look
  value_type bit_shift;   // The bit-wise offset within that future word
};

/// @brief Parallel, vectorised bitwise prime-difference counting over memory
/// segments.
/// @details This implementation translates the pair-counting problem into
/// parallel bitwise operations. The optimization is based on three main
/// concepts:
/// - Bit-vector primes: for a segment starting at an even value start, bit i
/// represents the odd integer v = start + 2*i + 1. The i-th bit is 1 iff v is
/// prime (as provided by primesieve). The bit-vector is stored as a vector of
/// words.
/// - Bitwise difference testing: to count occurrences of an even difference h
/// within a segment, we require v and v+h to be prime. This is equivalent to
/// checking bits i and i + h/2. Conceptually, we shift the bit-vector by s =
/// h/2 and perform a bitwise AND with the unshifted vector; the number of set
/// bits in the result (popcount) yields the number of valid prime pairs within
/// that 64‑bit word. This enables evaluating BITS_PER_WORD candidate integers
/// in parallel.
/// - Cache-aware tiling and boundary handling: shifts may cross word
/// boundaries, so each shift is decomposed into a whole-word component plus a
/// sub-word component. A pre-fetched lookahead buffer contains the next
/// segment's leading words to ensure shifts remain valid across segment
/// boundaries, removing conditional bounds checks in the hot loop. Compile-time
/// loop tiling processes multiple differences concurrently (tile size fixed via
/// templates), keeping intermediate counts in registers. Active segment sizes
/// are chosen as powers of two to fit the target cache (L1 on the development
/// hardware; performance is hardware-dependent and sensitive to cache
/// topology).
class Worker_optimized : public WorkerBase {
 public:
  /// @brief Set the state of the worker.
  explicit Worker_optimized(worker_state& state);

  void run() override final;

  static std::string get_parameter_description();

 private:
  /// @brief For each bit i in the given segment, set it so that segment[i] is 1
  /// if and only if seg_index * segment_size_ + 2 * i + 1 is prime.
  void sieve_segment(span_type segment, value_type seg_index);

  /// @brief Count each bit in the current segment (curr) that should be
  /// counted.
  void process_segment(span_type curr, span_const_type next, value_type start, value_type end);

  /// @brief For each difference being counted, count the number of bits in
  /// count_seg where the corresponding bit in lookahead_seg is also 1. It is
  /// likely that we will not need to count all bits in the first word of
  /// count_seg, so mask using last_word_mask to ensure we only count the
  /// required values.
  void accumulate(span_const_type count_seg, span_const_type lookahead_seg,
                  value_type last_word_mask);

  // Template helper for compile-time tile size optimization
  template <size_t TILE_SIZE>
  void accumulate_fixed_tile(span_const_type count_seg, span_const_type lookahead_seg,
                             value_type last_word_mask);

  // The size of each segment to process.
  value_type segment_size_;

  // The number of differences to consider in one sweep of a segment.
  value_type diff_tile_size_;

  // Each segment has space for max_lookahead_ words of the next segment's data
  // to minimise branching in accumulate.
  // max_lookahead_ is the maximum number of words from the next semgent that
  // need to be considered to count primes in the current segment. to consider.
  value_type max_lookahead_;
  // To detect a difference of size g starting near the end of the current
  // buffer, we need access to the beginning of the next buffer. We maintain TWO
  // active segments in memory at all times (only marking the primes that we
  // need).
  vector_type current_segment_;
  vector_type next_segment_;

  // Bitwise shift parameters for every requested difference.
  std::vector<difference_info> difference_info_;

  // Iterator to use to quickly generate primes in the required range.
  primesieve::iterator it_;
  value_type prime_;
};
