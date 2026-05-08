// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include <cstring>

#include "bits.hpp"
#include "doctest.h"
#include "types.hpp"

// Tests that check the bit manipulation operations.

TEST_CASE("Single Bit Operations") {
  // Three words, all 0
  vector_type data(3, ZERO);

  // Check setting and clearing bits near the ends of words
  vector_type test_bits = {
      0,                      // First bit of first word
      BITS_PER_WORD - 1,      // Last bit of first word
      BITS_PER_WORD,          // First bit of second word
      2 * BITS_PER_WORD - 1,  // Last bit of second word
      2 * BITS_PER_WORD,      // First bit of third word
      3 * BITS_PER_WORD - 1   // Last bit of third word
  };

  for (size_t i = 0; i < test_bits.size(); i++) {
    CHECK(get_bit(data, test_bits[i]) == false);
    set_bit(data, test_bits[i]);
    CHECK(get_bit(data, test_bits[i]) == true);
    clear_bit(data, test_bits[i]);
    CHECK(get_bit(data, test_bits[i]) == false);
  }

  // Check that setting doesn't affect nearby bits
  set_bit(data, BITS_PER_WORD / 2);
  CHECK(get_bit(data, BITS_PER_WORD / 2) == true);
  CHECK(get_bit(data, BITS_PER_WORD / 2 - 1) == false);
  CHECK(get_bit(data, BITS_PER_WORD / 2 + 1) == false);
}

TEST_CASE("Left Masking") {
  // Three words, all ones
  vector_type data(3, NOT_ZERO);

  // Number of bits to left-mask
  value_type limit = BITS_PER_WORD - 10;

  SUBCASE("Clear some bits of first word") {
    clear_left_bits(data, limit);

    //  Ensure early bits are zeroed out
    for (value_type i = 0; i < limit; i++) {
      CHECK(get_bit(data, i) == false);
    }
    // Ensure later bits are still set
    CHECK(get_bit(data, limit) == true);
    CHECK(get_bit(data, limit + 1) == true);

    // Ensure later words are not touched
    CHECK(data[1] == NOT_ZERO);
    CHECK(data[2] == NOT_ZERO);
  }

  SUBCASE("Clear first word") {
    limit = BITS_PER_WORD;
    clear_left_bits(data, BITS_PER_WORD);
    CHECK(data[0] == 0);         // First word empty
    CHECK(data[1] == NOT_ZERO);  // Remaining words full
    CHECK(data[2] == NOT_ZERO);
  }

  SUBCASE("Clear some of second word as well") {
    limit = BITS_PER_WORD + 10;
    clear_left_bits(data, limit);
    CHECK(data[0] == 0);
    CHECK(data[2] == NOT_ZERO);
    CHECK(get_bit(data, limit - 1) == false);
    CHECK(get_bit(data, limit) == true);
  }
}

TEST_CASE("Right Masking") {
  // Three words, all ones
  vector_type data(3, NOT_ZERO);

  // Number of bits to right-mask
  value_type limit = BITS_PER_WORD - 10;

  SUBCASE("Clear some bits of last word") {
    clear_right_bits(data, limit);

    //  Ensure late bits are zeroed out
    for (value_type i = 3 * BITS_PER_WORD - limit; i < 3 * BITS_PER_WORD; i++) {
      CHECK(get_bit(data, i) == false);
    }
    // Ensure earlier bits are still set
    CHECK(get_bit(data, 3 * BITS_PER_WORD - limit - 1) == true);

    // Ensure earlier words are not touched
    CHECK(data[0] == NOT_ZERO);
    CHECK(data[1] == NOT_ZERO);
  }

  SUBCASE("Clear last word") {
    limit = BITS_PER_WORD;
    clear_right_bits(data, BITS_PER_WORD);
    CHECK(data[0] == NOT_ZERO);  // First two words full
    CHECK(data[1] == NOT_ZERO);
    CHECK(data[2] == 0);  // Last word empty
  }

  SUBCASE("Clear some of second word as well") {
    limit = BITS_PER_WORD + 10;
    clear_right_bits(data, limit);
    CHECK(data[0] == NOT_ZERO);
    CHECK(data[2] == 0);
    CHECK(get_bit(data, 3 * BITS_PER_WORD - limit - 1) == true);
    CHECK(get_bit(data, 3 * BITS_PER_WORD - limit) == false);
  }
}

TEST_CASE("Shifting Bits") {
  SUBCASE("Perfect alignment (shift 4") {
    value_type left = 0xF0;   // 11110000
    value_type right = 0x0F;  // 00001111
    value_type bit_shift = 4;
    value_type result = shift_bits(left, right, bit_shift);
    CHECK((result & 0xFF) == 0x0F);
  }

  SUBCASE("Boundary stitching") {
    value_type left = ONE << BITS_PER_WORD_MINUS_ONE;  // Top bit set
    value_type right = ONE;                            // Bottom bit set
    value_type bit_shift = BITS_PER_WORD_MINUS_ONE;
    value_type result = shift_bits(left, right, bit_shift);
    CHECK(result == 3);
  }

  SUBCASE("Shift by 1") {
    // Simulate two adjacent words in a bit vector
    value_type left;
    std::memset(&left, 0xAA, BYTES_PER_WORD);  // Alternating bits
    value_type right;
    std::memset(&right, 0x55, BYTES_PER_WORD);  // Opposite pattern
    value_type result = shift_bits(left, right, 1);

    // Expected: top 63 bits of left, plus top 1 bit of right
    value_type expected = (left >> ONE) | (right << BITS_PER_WORD_MINUS_ONE);
    CHECK(result == expected);
  }

  SUBCASE("Shift by maximum (BITS_PER_WORD - 1)") {
    value_type left = ONE << BITS_PER_WORD_MINUS_ONE;
    value_type right = NOT_ZERO;
    value_type result = shift_bits(left, right, BITS_PER_WORD_MINUS_ONE);
    CHECK(result == (ONE | (NOT_ZERO << 1)));
  }

  SUBCASE("All zeros") { CHECK(shift_bits(0, 0, 10) == 0); }

  SUBCASE("Shift Zero") {
    value_type left = 0xDEADBEEF;
    value_type right = 0xCAFEBABE;

    // When bit_shift = 0, result should equal left (no shift)
    value_type result = shift_bits(left, right, 0);
    CHECK(result == left);

    // Verify right doesn't contaminate the result
    CHECK(result == 0xDEADBEEF);
  }
}

TEST_CASE("Clear All Bits") {
  vector_type data(5, NOT_ZERO);  // All bits set
  clear_all_bits(data);

  for (const auto word : data) {
    CHECK(word == 0);
  }
}
