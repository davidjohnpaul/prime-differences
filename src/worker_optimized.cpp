// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "worker_optimized.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "bits.hpp"
#include "parsing.hpp"
#include "sys_info.hpp"
#include "types.hpp"
#include "utils.hpp"

/// @brief By default, use a segment size is a power of 2 that fits in L2 cache.
[[nodiscard]] static value_type get_suggested_segment_size() {
  auto result = get_cache_sizes().l2;
  result = std::bit_floor(result);
  return result;
}

/// @brief By default, 2 seems to give best performance.
[[nodiscard]] static value_type get_suggested_difference_tile_size() { return 2; }

// Validates segment_size
Worker_optimized::Worker_optimized(worker_state& state) : WorkerBase(state) {
  if (state_.parameters.empty()) {
    segment_size_ = get_suggested_segment_size();
    diff_tile_size_ = get_suggested_difference_tile_size();
  } else {
    try {
      auto values = parse_vector_type(state_.parameters);
      if (values.empty()) {
        throw std::runtime_error("Parameters cannot be empty");
      }
      if (values.size() > 2) {
        throw std::runtime_error("Too many parameters");
      }
      segment_size_ = values[0];
      if (values.size() > 1) {
        diff_tile_size_ = values[1];
      } else {
        diff_tile_size_ = get_suggested_difference_tile_size();
      }
    } catch (std::runtime_error& e) {
      throw runtime_error(
          std::format("Parameters should be SEGMENT_SIZE[,DIFFERENCE_TILE_SIZE]. Segment "
                      "size must be a power of 2 greater than the maximum difference. "
                      "DIFFERENCE_TILE_SIZE must be one of 1, 2, 4, 8, 12, 16, 32, 64, "
                      "128. "
                      "Error: {}",
                      e.what()));
    }
  }

  // We count by checking values in the current and next segment, so we need all
  // necessary primes to be in one of those two. If the largest difference is
  // greater than or equal to segment_size, we would need to look into a third
  // segment, which means this algorithm wouldn't work.
  if (state_.differences.back() >= segment_size_) {
    throw runtime_error(
        std::format("Segment size (currently {}) must be larger than "
                    "the largest difference (currently {})",
                    segment_size_, state_.differences.back()));
  }
  // Our logic relies on a segment being at least one word long.
  if (segment_size_ < VALUES_PER_WORD) {
    throw runtime_error(std::format("Segment size (currently {}) must be at least {}",
                                    segment_size_, VALUES_PER_WORD));
  }
  // Compiler can only optimise if segment_size is a power of 2, so make sure it
  // is
  if (!std::has_single_bit(segment_size_)) {
    throw runtime_error(
        std::format("Segment size must be a power of 2. Currently {}", segment_size_));
  }

  // Check that the largest value we'll need + segment_size_ won't overflow
  if (std::numeric_limits<value_type>::max() - segment_size_ <
      state_.end + state_.differences.back()) {
    throw runtime_error(std::format(
        "Segment size (currently {}) is too large for the requested range", segment_size_));
  }

  // Ensure a positive tile size
  if (diff_tile_size_ == 0) {
    throw runtime_error("Tile size must be positive");
  }

  // Ensure tile size is not too large
  if (diff_tile_size_ > MAX_DIFF_TILE_SIZE) {
    throw runtime_error(
        std::format("Tile size must be less than or equal to {}", MAX_DIFF_TILE_SIZE));
  }

  // Pre-calculate shift info for our even differences
  max_lookahead_ = 0;
  for (auto difference : state_.differences) {
    auto bit_offset = difference / 2;              // We're only considering odd numbers
    auto word_shift = bit_offset / BITS_PER_WORD;  // Number of words to shift
    auto bit_shift =
        bit_offset % BITS_PER_WORD;  // Number of bits to shift once we've shifted by words
    difference_info diff_info{word_shift, bit_shift};
    difference_info_.push_back(diff_info);
    // Determine how may words we need to look ahead - full words to skip from
    // the word shift, plus one for the potentially next incomplete word.
    value_type required_lookahead = diff_info.word_shift + 1;
    max_lookahead_ = std::max(max_lookahead_, required_lookahead);
  }
  // Calculate exact word count needed for this range
  value_type num_words = ((segment_size_ / 2) + BITS_PER_WORD_MINUS_ONE) / BITS_PER_WORD;
  // Ensure the current and next segments have enough words for the entire
  // segment and the required lookahead
  current_segment_.resize(num_words + max_lookahead_);
  next_segment_.resize(current_segment_.size());

  // Get the first prime to consider - jump_to is a slow operation, so we don't
  // want to do it too often!
  it_.jump_to(state_.current_position);
  prime_ = it_.next_prime();

  // Handle 2
  if (prime_ == 2) {
    if (state_.differences[0] == 0 && state_.end > prime_) {
      state_.current_counts[0]++;
    }
    state_.current_position = std::min(state_.end, prime_ + ONE);
    prime_ = it_.next_prime();
  }
}

void Worker_optimized::run() {
  if (state_.current_position >= state_.end) {
    return;  // Already complete
  }

  // Calculate our current segment
  value_type segment_index = state_.current_position / segment_size_;

  // We check current and next segment, so generate first current segment before
  // main loop, and always generate next segment in the loop.
  sieve_segment(current_segment_, segment_index);

  // Main loop
  while (state_.current_position < state_.end) {
    // Check global interrupt flag (CTRL+C)
    if (!KEEP_RUNNING.test(std::memory_order_relaxed)) [[unlikely]] {
      break;
    }

    value_type start = segment_index * segment_size_;
    value_type end = start + segment_size_;

    // Generate next segment
    segment_index++;
    sieve_segment(next_segment_, segment_index);

    // Process current segment (using next for lookahead)
    process_segment(current_segment_, next_segment_, start, end);

    // Update Progress
    state_.current_position = std::min(end, state_.end);

    // Swap next and current. We've finished with current, so it can be
    // overwritten.
    std::swap(current_segment_, next_segment_);

    // Progress and checkpoint check
    output_progress();
  }
}

void Worker_optimized::sieve_segment(span_type segment, value_type seg_index) {
  auto start = seg_index * segment_size_;
  auto end = start + segment_size_;

  // Find next prime after start of segment.
  while (prime_ < start) [[unlikely]] {
    prime_ = it_.next_prime();
  }

  // Set all values to composite (0).
  clear_all_bits(segment);

  // Set each prime at bit index (prime - start) / 2 to 1.
  while (prime_ < end && prime_ <= state_.end + state_.differences.back()) {
    auto idx = (prime_ - start) / 2;
    set_bit(segment, idx);
    prime_ = it_.next_prime();
  }
}

void Worker_optimized::process_segment(span_type curr, span_const_type next, value_type start,
                                       value_type end) {
  // Copy required part of next segment to lookahead of current
  value_type logical_size = static_cast<value_type>(curr.size() - max_lookahead_);
  std::ranges::copy(next.subspan(0, max_lookahead_),
                    curr.subspan(logical_size, max_lookahead_).begin());

  // Find the bits we need to count and the bits we need for lookahead
  // Since segment_size_ is a power of two, we know start and end are even
  auto first_odd_in_segment = start + 1;
  auto last_odd_in_segment = end - 1;

  // The first odd value we need to consider
  auto first_odd_wanted = std::max(first_odd_in_segment, (state_.current_position % 2 == 0)
                                                             ? state_.current_position + 1
                                                             : state_.current_position);
  // The last odd value we need to count
  auto last_odd_to_count =
      std::min(last_odd_in_segment, (state_.end % 2) == 0 ? state_.end - 1 : state_.end - 2);

  if (last_odd_to_count < first_odd_wanted) {
    return;  // No odds to count in range
  }

  // Get the bit and word index for the first odd value we need to count
  auto first_bit_index = (first_odd_wanted - first_odd_in_segment) / 2;
  auto first_word_index = first_bit_index / BITS_PER_WORD;

  // Get the bit and word index for the last odd value we need to count
  auto last_bit_index_to_count = (last_odd_to_count - first_odd_in_segment) / 2;
  auto last_word_index_to_count = last_bit_index_to_count / BITS_PER_WORD;

  // Determine whether we need to skip any bits in the first/last word to count
  auto count_from_bit_position_in_first_word = first_bit_index % BITS_PER_WORD;
  auto count_to_bit_position_in_last_word = last_bit_index_to_count % BITS_PER_WORD;

  // The number of words we need to consider (we may not need all bits in the
  // first and last word)
  auto count_length = last_word_index_to_count - first_word_index + 1;

  // Create a view that can only see the words we need to count
  span_type count_view = curr.subspan(first_word_index, count_length);

  // Create a view that can only see the words we need to lookahead
  span_type lookahead_view = curr.subspan(first_word_index);

  // If we don't need all of the first word, clear out anything we don't need
  if (count_from_bit_position_in_first_word > 0) {
    clear_left_bits(count_view, count_from_bit_position_in_first_word);
  }

  // We can't clear out the right side the same way because we may need those
  // bits for lookahead. Instead, we'll mask the last word when counting.
  // Note that 1 + count_to_bit_position_in_last_word might equal BITS_PER_WORD
  // and shifting by BITS_PER_WORD is undefined behaviour. So we need to first
  // shift by 1, then shift by count_to_bit_position_in_last_word
  auto last_word_to_count_mask = ((ONE << ONE) << count_to_bit_position_in_last_word) - ONE;

  // Do the count
  accumulate(count_view, lookahead_view, last_word_to_count_mask);
}

template <size_t TILE_SIZE>
void Worker_optimized::accumulate_fixed_tile(span_const_type count_seg,
                                             span_const_type lookahead_seg,
                                             value_type last_word_mask) {
  const size_t num_diffs = difference_info_.size();

  // Process all full tiles
  const size_t num_full_tiles = num_diffs / TILE_SIZE;

  for (size_t tile_num = 0; tile_num < num_full_tiles; tile_num++) {
    const size_t tile_start = tile_num * TILE_SIZE;

    // Fixed-size array at compile time, so can be kept in registers
    alignas(BITS_PER_WORD) std::array<value_type, TILE_SIZE> tile_counts{};

    // Process complete words - the final partial word is processed below
    for (size_t i = 0; i < count_seg.size() - 1; i++) {
      value_type base_word = count_seg[i];

      for (size_t idx = 0; idx < TILE_SIZE; idx++) {
        const size_t diff_idx = tile_start + idx;
        const auto& [word_shift, bit_shift] = difference_info_[diff_idx];

        value_type shifted =
            shift_bits(lookahead_seg[i + word_shift], lookahead_seg[i + word_shift + 1], bit_shift);

        tile_counts[idx] += static_cast<unsigned>(std::popcount(base_word & shifted));
      }
    }

    // Process final word with mask
    {
      size_t i = count_seg.size() - 1;
      value_type base_word = count_seg[i] & last_word_mask;

      for (size_t idx = 0; idx < TILE_SIZE; idx++) {
        const size_t diff_idx = tile_start + idx;
        const auto& [word_shift, bit_shift] = difference_info_[diff_idx];

        value_type shifted =
            shift_bits(lookahead_seg[i + word_shift], lookahead_seg[i + word_shift + 1], bit_shift);

        tile_counts[idx] += static_cast<unsigned>(std::popcount(base_word & shifted));
      }
    }

    // Write back to global counts
    for (size_t idx = 0; idx < TILE_SIZE; idx++) {
      state_.current_counts[tile_start + idx] += tile_counts[idx];
    }
  }

  // Handle remainder (partial tile) if any
  const size_t remainder = num_diffs % TILE_SIZE;
  if (remainder > 0) {
    const size_t tile_start = num_full_tiles * TILE_SIZE;
    alignas(BITS_PER_WORD) std::array<value_type, TILE_SIZE> tile_counts{};

    // Process complete words
    for (size_t i = 0; i < count_seg.size() - 1; i++) {
      value_type base_word = count_seg[i];

      for (size_t idx = 0; idx < remainder; idx++) {
        const size_t diff_idx = tile_start + idx;
        const auto& [word_shift, bit_shift] = difference_info_[diff_idx];

        value_type shifted =
            shift_bits(lookahead_seg[i + word_shift], lookahead_seg[i + word_shift + 1], bit_shift);

        tile_counts[idx] += static_cast<unsigned>(std::popcount(base_word & shifted));
      }
    }

    // Process final word with mask
    {
      size_t i = count_seg.size() - 1;
      value_type base_word = count_seg[i] & last_word_mask;

      for (size_t idx = 0; idx < remainder; idx++) {
        const size_t diff_idx = tile_start + idx;
        const auto& [word_shift, bit_shift] = difference_info_[diff_idx];

        value_type shifted =
            shift_bits(lookahead_seg[i + word_shift], lookahead_seg[i + word_shift + 1], bit_shift);

        tile_counts[idx] += static_cast<unsigned>(std::popcount(base_word & shifted));
      }
    }

    // Write back
    for (size_t idx = 0; idx < remainder; idx++) {
      state_.current_counts[tile_start + idx] += tile_counts[idx];
    }
  }
}

void Worker_optimized::accumulate(span_const_type count_seg, span_const_type lookahead_seg,
                                  value_type last_word_mask) {
  // Dispatch to template based on configured tile size
  switch (diff_tile_size_) {
    case 1:
      accumulate_fixed_tile<1>(count_seg, lookahead_seg, last_word_mask);
      break;
    case 2:
      accumulate_fixed_tile<2>(count_seg, lookahead_seg, last_word_mask);
      break;
    case 4:
      accumulate_fixed_tile<4>(count_seg, lookahead_seg, last_word_mask);
      break;
    case 8:
      accumulate_fixed_tile<8>(count_seg, lookahead_seg, last_word_mask);
      break;
    case 12:
      accumulate_fixed_tile<12>(count_seg, lookahead_seg, last_word_mask);
      break;
    case 16:
      accumulate_fixed_tile<16>(count_seg, lookahead_seg, last_word_mask);
      break;
    case 32:
      accumulate_fixed_tile<32>(count_seg, lookahead_seg, last_word_mask);
      break;
    case 64:
      accumulate_fixed_tile<64>(count_seg, lookahead_seg, last_word_mask);
      break;
    case 128:
      accumulate_fixed_tile<128>(count_seg, lookahead_seg, last_word_mask);
      break;
    default:
      throw runtime_error(
          std::format("Unsupported tile size: {}. "
                      "Supported sizes: 1, 2, 4, 8, 12, 16, 32, 64, 128.",
                      diff_tile_size_));
  }
}

std::string Worker_optimized::get_parameter_description() {
  return std::format(
      "is SEGMENT_SIZE[,DIFFERENCE_TILE_SIZE] "
      "where:\n\t{:<30}\t{}\n\t{:<30}\t{}",
      "SEGMENT_SIZE",
      std::format("is the size (in words) of each segment to consider (defaults to {} "
                  "({} KB) on this system).",
                  get_suggested_segment_size(),
                  BYTES_PER_WORD * get_suggested_segment_size() / 1024),
      "DIFFERENCE_TILE_SIZE",
      std::format("is the number of differences to consider in one sweep of a "
                  "segment (supported sizes: 1, 2, 4, 8, 12, 16, 32, 64, 128; "
                  "defaults to {}).",
                  get_suggested_difference_tile_size()));
}
