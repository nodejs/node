//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains a two-level segregated fit table and mapping helper.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_TLSF_TABLE_H
#define LLVM_LIBC_SRC___SUPPORT_TLSF_TABLE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/block.h"
#include "src/__support/freelist.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math_extras.h"

namespace LIBC_NAMESPACE_DECL {

/// Default configuration for TLSFFreeStore and TLSFTable.
struct DefaultFreeStoreConfig {
  static constexpr size_t UNIT_SIZE = BlockRef::MIN_ALIGN;
  static constexpr size_t STEP_SIZE_BITS = 3;
  static constexpr size_t NUM_STEP_BITS = 2;
  static constexpr size_t NUM_TABLE_ENTRIES = sizeof(uintptr_t) == 8 ? 3 : 6;
  static constexpr bool USE_TRIE_FOR_OVERFLOW_BIN = true;
  static constexpr size_t LINEAR_SCAN_LIMIT = 16;
};

// A two-level segregated fit occupancy table and mapping helper.
//
// Rather than mapping raw block sizes directly, the table maps the size delta
// over MIN_INNER_SIZE (delta = size - (MIN_INNER_SIZE + 1)). Small allocations
// map directly to exact-size linear bins covering Bins 0 through EXP_BASE (33
// linear bins total). Larger allocations are managed in a 2-D table starting at
// Bin 33. One can think of each row containing NUM_STEPS lists. Along the row,
// the delta grows exponentially by powers of 2; along the column, the delta
// increases linearly by STEP_SIZE increments.
//
// Mathematical layout:
//   delta = size <= MIN_INNER_SIZE ? 0 : size - (MIN_INNER_SIZE + 1)
//   STEP_SIZE = 1 << STEP_SIZE_BITS
//   NUM_STEPS = 1 << NUM_STEP_BITS
//   EXP_BASE = STEP_SIZE * NUM_STEPS
//   LINEAR_BINS = EXP_BASE + 1
//   LARGE_DELTA_THRESHOLD = UNIT_SIZE * EXP_BASE
//
// Visual representation with realistic configuration parameters:
//   MIN_INNER_SIZE = 24 B, UNIT_SIZE = 16, STEP_SIZE = 8, NUM_STEPS = 4
//   EXP_BASE = 32, LINEAR_BINS = 33, THRESHOLD = 512 B (Size 536 B)
//
// 1. Small Sizes (Linear Bins on Delta over MIN_INNER_SIZE = 24 B):
//    Covers size range [0, ... 536 B] across Bins 0 to 32 (EXP_BASE = 32).
//    Assuming UNIT_SIZE matches block minimum alignment (step = unit = 16 B),
//    these linear bins hold blocks of exact inner sizes (e.g., Bin 0 = 24 B,
//    Bin 1 = 40 B, Bin 2 = 56 B). The byte ranges shown below reflect user
//    payload sizes that map directly into each exact-fit bin:
// +-----------+-------------+-------------+-----+--------------+--------------+
// |   Bin 0   |    Bin 1    |    Bin 2    | ... |    Bin 31    |    Bin 32    |
// | [0..24 B] |  [25..40 B] |  [41..56 B] | ... | [505..520 B] | [521..536 B] |
// | (Delta 0) |  (D: 0..15) | (D: 16..31) | ... | (D: 480..495)| (D: 496..511)|
// +-----------+-------------+-------------+-----+--------------+--------------+
//
// 2. Large Sizes (2-D Table on Delta over MIN_INNER_SIZE for Sizes > 536 B):
//    Rows = FL (Exponential growth), Columns = SL (Linear steps)
//    Each cell shows Bin index, exact Block Size range, and Delta (D) range:
//
//                       LINEAR INCREASE ALONG COLUMN (SL) --->
//             +---------------+---------------+---------------+---------------+
//             |    Col = 0    |    Col = 1    |    Col = 2    |    Col = 3    |
//             |    (Base)     |  (+25% Step)  |  (+50% Step)  |  (+75% Step)  |
//   +---------+---------------+---------------+---------------+---------------+
// E | Row = 0 |    Bin 33     |    Bin 34     |    Bin 35     |    Bin 36     |
// X | Base D: | [537..664 B]  | [665..792 B]  | [793..920 B]  | [921..1048 B] |
// P |   512   | D:[512..639]  | D:[640..767]  | D:[768..895]  | D:[896..1023] |
//   +---------+---------------+---------------+---------------+---------------+
// O | Row = 1 |    Bin 37     |    Bin 38     |    Bin 39     |    Bin 40     |
// N | Base D: |[1049..1304 B] |[1305..1560 B] |[1561..1816 B] |[1817..2072 B] |
// E |  1024   |D:[1024..1279] |D:[1280..1535] |D:[1536..1791] |D:[1792..2047] |
//   +---------+---------------+---------------+---------------+---------------+
// N | Row = 2 |    Bin 41     |    Bin 42     |    Bin 43     |    Bin 44     |
// T | Base D: |[2073..2584 B] |[2585..3096 B] |[3097..3608 B] |[3609..4120 B] |
// I |  2048   |D:[2048..2559] |D:[2560..3071] |D:[3072..3583] |D:[3584..4095] |
//   +---------+---------------+---------------+---------------+---------------+
// A | Row = 3 |    Bin 45     |    Bin 46     |    Bin 47     |    Bin 48     |
// L | Base D: |[4121..5144 B] |[5145..6168 B] |[6169..7192 B] |[7193..8216 B] |
//   |  4096   |D:[4096..5119] |D:[5120..6143] |D:[6144..7167] |D:[7168..8191] |
//   +---------+---------------+---------------+---------------+---------------+
//
// Note: For the real implementation, we don't actually store the lists in a
// 2-D structure. Instead, we flatten the entire 2-D layout into a single
// flat 1-D array of size TOTAL_BINS, and map sizes directly to a continuous
// 1-D index using size_to_bin. The occupancy state is tracked compactly in
// the lookup_table bitmask array.
template <typename CONFIG> class TLSFTable {
protected:
  static_assert(cpp::has_single_bit(CONFIG::UNIT_SIZE),
                "unit size must be a power of two");
  static_assert(CONFIG::NUM_TABLE_ENTRIES > 0,
                "the lookup table must have at least one entry");

  static constexpr size_t STEP_SIZE = size_t(1) << CONFIG::STEP_SIZE_BITS;
  static constexpr size_t NUM_STEPS = size_t(1) << CONFIG::NUM_STEP_BITS;
  static constexpr size_t EXP_BASE = STEP_SIZE * NUM_STEPS;
  static constexpr int UNIT_SIZE_LOG2 = cpp::bit_width(CONFIG::UNIT_SIZE) - 1;
  static constexpr int EXP_BASE_LOG2 =
      CONFIG::STEP_SIZE_BITS + CONFIG::NUM_STEP_BITS;
  static constexpr size_t BITS_PER_ENTRY =
      cpp::numeric_limits<uintptr_t>::digits;

public:
  static constexpr size_t TOTAL_BINS =
      CONFIG::NUM_TABLE_ENTRIES * BITS_PER_ENTRY;
  static constexpr size_t TOTAL_BITS = TOTAL_BINS;

  static constexpr size_t MIN_OUTER_SIZE = align_up(
      BlockRef::HEADER_SIZE + sizeof(FreeList::Node), BlockRef::MIN_ALIGN);
  // Minimal available size for allocation.
  static constexpr size_t MIN_INNER_SIZE =
      MIN_OUTER_SIZE - BlockRef::HEADER_SIZE + BlockRef::PREV_FIELD_SIZE;
  // Number of bins grows linearly.
  static constexpr size_t LINEAR_BINS = EXP_BASE + 1;

  LIBC_INLINE constexpr TLSFTable() = default;

  LIBC_INLINE static constexpr size_t size_to_bin(size_t size);
  LIBC_INLINE static constexpr size_t bin_to_min_size(size_t bin);

  LIBC_INLINE void mark_occupied(size_t bin) {
    size_t entry_index = bin / BITS_PER_ENTRY;
    size_t bit_offset = bin % BITS_PER_ENTRY;
    lookup_table[entry_index] |= uintptr_t(1) << bit_offset;
  }

  LIBC_INLINE void mark_vacant(size_t bin) {
    size_t entry_index = bin / BITS_PER_ENTRY;
    size_t bit_offset = bin % BITS_PER_ENTRY;
    lookup_table[entry_index] &= ~(uintptr_t(1) << bit_offset);
  }

  LIBC_INLINE bool is_occupied(size_t bin) const {
    size_t entry_index = bin / BITS_PER_ENTRY;
    size_t bit_offset = bin % BITS_PER_ENTRY;
    return (lookup_table[entry_index] & (uintptr_t(1) << bit_offset)) != 0;
  }

  LIBC_INLINE size_t find_first_occupied_after(size_t bin) const {
    if (bin >= TOTAL_BINS - 1)
      return TOTAL_BINS;

    size_t target_index = bin + 1;
    size_t start_entry = target_index / BITS_PER_ENTRY;
    size_t bit_offset = target_index % BITS_PER_ENTRY;

    uintptr_t value = lookup_table[start_entry] & (~uintptr_t(0) << bit_offset);
    if (value != 0)
      return start_entry * BITS_PER_ENTRY +
             static_cast<size_t>(cpp::countr_zero(value));

    for (size_t i = start_entry + 1; i < CONFIG::NUM_TABLE_ENTRIES; ++i) {
      value = lookup_table[i];
      if (value != 0)
        return i * BITS_PER_ENTRY +
               static_cast<size_t>(cpp::countr_zero(value));
    }
    return TOTAL_BINS;
  }

private:
  cpp::array<uintptr_t, CONFIG::NUM_TABLE_ENTRIES> lookup_table{};
};

template <typename CONFIG>
LIBC_INLINE constexpr size_t TLSFTable<CONFIG>::size_to_bin(size_t size) {
  // Compute bin as delta on top of min_inner_size
  if (sub_overflow(size, MIN_INNER_SIZE + 1, size))
    return 0;

  if (size < (EXP_BASE << UNIT_SIZE_LOG2))
    return (size >> UNIT_SIZE_LOG2) + 1;

  size_t size_ilog2 = static_cast<size_t>(cpp::bit_width(size) - 1);
  size_t exp_offset = (size_ilog2 - UNIT_SIZE_LOG2 - EXP_BASE_LOG2 - 1)
                      << CONFIG::NUM_STEP_BITS;
  size_t step_index = size >> (size_ilog2 - CONFIG::NUM_STEP_BITS);
  size_t index = LINEAR_BINS + exp_offset + step_index;

  return index < TOTAL_BINS ? index : TOTAL_BINS - 1;
}

template <typename CONFIG>
LIBC_INLINE constexpr size_t TLSFTable<CONFIG>::bin_to_min_size(size_t bin) {
  if (bin == 0)
    return 0;
  if (bin < LINEAR_BINS)
    return MIN_INNER_SIZE + 1 + ((bin - 1) << UNIT_SIZE_LOG2);

  size_t local_index = bin - LINEAR_BINS;
  size_t exp_index = local_index >> CONFIG::NUM_STEP_BITS;
  size_t linear_index = local_index & (NUM_STEPS - 1);

  size_t row_base = (EXP_BASE << exp_index) << UNIT_SIZE_LOG2;
  size_t step_size = (STEP_SIZE << exp_index) << UNIT_SIZE_LOG2;
  return MIN_INNER_SIZE + 1 + row_base + linear_index * step_size;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_TLSF_TABLE_H
