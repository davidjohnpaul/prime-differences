// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#pragma once
#include "types.hpp"

// Bit manipulation operations

// Note: These functions assume a Little-Endian bit ordering:
// - Index 0 of a span is the base number.
// - Bit 0 of Word 0 is the very first number.
// - Bit 63 of Word 0 is followed immediately by Bit 0 of Word 1.

/// @brief Reads a single bit from the large bit span.
/// @param a The span of values to read from.
/// @param i The absolute bit index to read.
/// For example, get_bit(a, BITS_PER_WORD + 3) will get the third bit of the
/// second word of a
[[nodiscard]] inline bool get_bit(span_const_type a, value_type i) noexcept {
  // Optimization: Compiler replaces division/mod usage with shift/and
  // when BITS_PER_WORD is a power of 2.
  return (a[i / BITS_PER_WORD] >> (i & BITS_PER_WORD_MINUS_ONE)) & ONE;
}

/// @brief Clears (sets to 0) a single bit.
/// For example, clear_bit(a, BITS_PER_WORD + 3) will set the third bit of the
/// second word of a to 0
inline void clear_bit(span_type a, value_type i) noexcept {
  a[i / BITS_PER_WORD] &= ~(ONE << (i & BITS_PER_WORD_MINUS_ONE));
}

/// @brief Sets (sets to 1) a single bit.
/// For example, clear_bit(a, BITS_PER_WORD + 3) will set the third bit of the
/// second word of a to 1
inline void set_bit(span_type a, value_type i) noexcept {
  a[i / BITS_PER_WORD] |= (ONE << (i & BITS_PER_WORD_MINUS_ONE));
}

/// @brief Sets every bit in the span to 0
inline void clear_all_bits(span_type a) noexcept { std::ranges::fill(a, 0); }

/// @brief Zeroes out the first n bits of the span.
inline void clear_left_bits(span_type a, value_type n) noexcept {
  // Clear complete words
  size_t full_words = n / BITS_PER_WORD;
  if (full_words > 0) {
    std::ranges::fill(a.subspan(0, full_words), 0);
  }

  // Clear remaining bits in the partial word
  size_t rem_bits = n % BITS_PER_WORD;
  if (rem_bits > 0) {
    // Create a mask with 1s only in the upper part
    // Example: If we want to clear lower 2 bits: Mask = 1111...1100
    value_type mask = NOT_ZERO << rem_bits;
    a[full_words] &= mask;
  }
}

/// @brief Zeroes out the last n bits of the span
inline void clear_right_bits(span_type a, value_type n) noexcept {
  // Clear complete words
  size_t full_words = n / BITS_PER_WORD;
  if (full_words > 0) {
    std::ranges::fill(a.subspan(a.size() - full_words), 0);
  }

  // Clear remaining bits in the partial word
  value_type rem_bits = n % BITS_PER_WORD;
  if (rem_bits > 0) {
    value_type boundary_idx = a.size() - full_words - ONE;

    // Keep lower bits
    value_type bits_to_keep = BITS_PER_WORD - rem_bits;
    value_type mask = (ONE << bits_to_keep) - ONE;
    a[boundary_idx] &= mask;
  }
}

/// @brief Left shifts the two words passed at parameters by bit_shift
/// positions.
/// @param left The left word to be processed.
/// @param right The right word to be processed.
/// @param bit_shift The offset relative to the current position.
/// @return A word representing the bitstream shifted by `bit_shift`.
[[nodiscard]] inline value_type shift_bits(value_type left, value_type right,
                                           value_type bit_shift) noexcept {
  // Result = (upper bit_shift bits of left) followed by remaining (lower bits
  // of right)

  // Get upper bits of left
  value_type shifted_left = left >> bit_shift;

  //  Get lower bits of right
  // Since bit_shift is anywhere in the range [0, BITS_PER_WORD), BITS_PER_WORD
  // - bit_shift may be BITS_PER_WORD (if bit_shift is 0). Since shifting by
  // BITS_PER_WORD results in undefined behaviour, instead shift by
  // (BITS_PER_WORD_MINUS_ONE - bit_shift), then shift by 1, which has the
  // expected behaviour without ever actually shifting by BITS_PER_WORD.
  value_type shifted_right = (right << (BITS_PER_WORD_MINUS_ONE - bit_shift)) << ONE;

  //  Or the two together
  return shifted_left | shifted_right;
}
