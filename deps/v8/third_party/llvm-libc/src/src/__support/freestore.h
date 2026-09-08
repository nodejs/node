//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains a two-level segregated fit free block store.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FREESTORE_H
#define LLVM_LIBC_SRC___SUPPORT_FREESTORE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/block.h"
#include "src/__support/freelist.h"
#include "src/__support/freetrie.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/tlsf_table.h"

namespace LIBC_NAMESPACE_DECL {

// A two-level segregated fit store for free blocks.
//
// See TLSFTable in tlsf_table.h for the mathematical layout and bin mapping
// logic.
//
// Generic memory workloads typically cluster around smaller allocation
// requests, exhibiting an inverse relationship between request frequency
// and block size. Furthermore, large allocations can suffer from severe
// internal fragmentation if managed with overly coarse-grained binning.
//
// To accommodate these workload dynamics, this store organizes free blocks
// into three specialized tiers, each employing a tailored management strategy:
//
// 1. Small Blocks (Exact-Fit Fast Path): Small blocks are maintained in linear,
//    exact-size bins. When a matching block is available, fast-path allocations
//    return an exact-fit block immediately in O(1) time without list traversal
//    or block splitting.
// 2. Medium Blocks (Two-Level Segregated Fit Table): Mid-sized blocks are
//    managed across an exponential and linear bin grid. To preserve constant
//    O(1) allocation time on the fast path, the allocator preferentially pops
//    from an oversized bin first, accepting minor block splitting overhead to
//    prevent memory waste while guaranteeing bounded search latency.
// 3. Large Blocks (Best-Fit Trie): For the coldest, largest block sizes, blocks
//    are stored in an ordered trie (when enabled). These allocations are
//    serviced using logarithmic best-fit searches, prioritizing space
//    efficiency and minimal fragmentation over immediate O(1) latency.
template <typename CONFIG> class TLSFFreeStoreImpl {
public:
  using Table = TLSFTable<CONFIG>;
  static constexpr size_t TOTAL_BITS = Table::TOTAL_BINS;
  static constexpr size_t TOTAL_BINS = Table::TOTAL_BINS;
  static constexpr size_t MIN_OUTER_SIZE = Table::MIN_OUTER_SIZE;
  static constexpr size_t MIN_INNER_SIZE = Table::MIN_INNER_SIZE;
  static constexpr size_t LINEAR_BINS = Table::LINEAR_BINS;
  static constexpr bool USE_TRIE = CONFIG::USE_TRIE_FOR_OVERFLOW_BIN;

  /// Integrity check for the entire store.
  LIBC_INLINE void integrity_check() const {
    if constexpr (USE_TRIE)
      trie.integrity_check();
    else
      overflow_list.integrity_check();
    for (const FreeList &list : free_lists)
      list.integrity_check();
  }

private:
  LIBC_INLINE constexpr TLSFFreeStoreImpl(cpp::bool_constant<true>) : trie() {}
  LIBC_INLINE constexpr TLSFFreeStoreImpl(cpp::bool_constant<false>)
      : overflow_list() {}

public:
  LIBC_INLINE constexpr TLSFFreeStoreImpl()
      : TLSFFreeStoreImpl(cpp::bool_constant<USE_TRIE>{}) {}
  LIBC_INLINE TLSFFreeStoreImpl(const TLSFFreeStoreImpl &other) = delete;
  LIBC_INLINE TLSFFreeStoreImpl &
  operator=(const TLSFFreeStoreImpl &other) = delete;

  LIBC_INLINE static constexpr size_t index_to_min_size(size_t index) {
    return Table::bin_to_min_size(index);
  }
  LIBC_INLINE void set_range(FreeTrie::SizeRange range);
  LIBC_INLINE void insert(BlockRef block);
  LIBC_INLINE void remove(BlockRef block);
  LIBC_INLINE BlockRef remove_best_fit(size_t size) {
    return find_and_remove_fit(size);
  }
  LIBC_INLINE BlockRef find_and_remove_fit(size_t size);
  LIBC_INLINE static constexpr size_t size_to_bit_index(size_t size) {
    return Table::size_to_bin(size);
  }

protected:
  LIBC_INLINE static bool too_small(BlockRef block) {
    return block.outer_size() < MIN_OUTER_SIZE;
  }

  Table free_sizes;
  cpp::array<FreeList, TOTAL_BITS - 1> free_lists;
  union {
    FreeTrie trie;
    FreeList overflow_list;
  };

  LIBC_INLINE BlockRef remove_first_fit_in_list(size_t index, size_t size);
  LIBC_INLINE BlockRef find_and_remove_fit_in_trie(size_t size);
};

template <typename CONFIG>
LIBC_INLINE void
TLSFFreeStoreImpl<CONFIG>::set_range(FreeTrie::SizeRange range) {
  if constexpr (USE_TRIE) {
    size_t heap_max = range.min + range.width;
    size_t overflow_min = index_to_min_size(TOTAL_BITS - 1);
    size_t width = 1;
    if (heap_max > overflow_min)
      width = cpp::bit_ceil(heap_max - overflow_min);
    trie.set_range(FreeTrie::SizeRange(overflow_min, width));
  }
}

template <typename CONFIG>
LIBC_INLINE BlockRef
TLSFFreeStoreImpl<CONFIG>::find_and_remove_fit_in_trie(size_t size) {
  if (FreeTrie::Node *best_fit = trie.find_best_fit(size)) {
    BlockRef block = best_fit->block();
    trie.remove(best_fit);
    if (trie.empty())
      free_sizes.mark_vacant(TOTAL_BITS - 1);
    return block;
  }
  return BlockRef();
}

template <typename CONFIG>
LIBC_INLINE void TLSFFreeStoreImpl<CONFIG>::insert(BlockRef block) {
  if (too_small(block))
    return;
  size_t bit_index = size_to_bit_index(block.inner_size());

  if (bit_index == TOTAL_BITS - 1) {
    if constexpr (USE_TRIE)
      trie.push(block);
    else
      overflow_list.push(block);
    free_sizes.mark_occupied(bit_index);
    return;
  }

  free_lists[bit_index].push(block);
  free_sizes.mark_occupied(bit_index);
}

template <typename CONFIG>
LIBC_INLINE void TLSFFreeStoreImpl<CONFIG>::remove(BlockRef block) {
  if (too_small(block))
    return;
  size_t bit_index = size_to_bit_index(block.inner_size());

  if (bit_index == TOTAL_BITS - 1) {
    if constexpr (USE_TRIE) {
      trie.remove(reinterpret_cast<FreeTrie::Node *>(block.usable_space()));
      if (trie.empty())
        free_sizes.mark_vacant(bit_index);
    } else {
      overflow_list.remove(
          reinterpret_cast<FreeList::Node *>(block.usable_space()));
      if (overflow_list.empty())
        free_sizes.mark_vacant(bit_index);
    }
    return;
  }

  free_lists[bit_index].remove(
      reinterpret_cast<FreeList::Node *>(block.usable_space()));
  if (free_lists[bit_index].empty())
    free_sizes.mark_vacant(bit_index);
}

template <typename CONFIG>
LIBC_INLINE BlockRef
TLSFFreeStoreImpl<CONFIG>::remove_first_fit_in_list(size_t index, size_t size) {
  FreeList &list =
      (index == TOTAL_BITS - 1) ? overflow_list : free_lists[index];
  FreeList::Node *begin_node = list.begin();
  if (begin_node == nullptr)
    return BlockRef();

  FreeList::Node *cur = begin_node;
  size_t count = 0;
  do {
    if (cur->size() >= size) {
      list.remove(cur);
      if (list.empty())
        free_sizes.mark_vacant(index);
      return cur->block();
    }
    cur = cur->next();
    ++count;
  } while (cur != begin_node && count < CONFIG::LINEAR_SCAN_LIMIT);

  return BlockRef();
}

template <typename CONFIG>
LIBC_INLINE BlockRef
TLSFFreeStoreImpl<CONFIG>::find_and_remove_fit(size_t size) {

  size_t bit_index = size_to_bit_index(size);

  // Fast path for small linear bins if UNIT_SIZE == MIN_ALIGN
  if constexpr (CONFIG::UNIT_SIZE == BlockRef::MIN_ALIGN) {
    if (LIBC_LIKELY(bit_index < LINEAR_BINS &&
                    free_sizes.is_occupied(bit_index))) {
      BlockRef block = free_lists[bit_index].front();
      free_lists[bit_index].pop();
      if (free_lists[bit_index].empty())
        free_sizes.mark_vacant(bit_index);
      return block;
    }
  }

  if (LIBC_UNLIKELY(bit_index >= TOTAL_BITS - 1)) {
    if constexpr (USE_TRIE)
      return find_and_remove_fit_in_trie(size);
    else
      return remove_first_fit_in_list(TOTAL_BITS - 1, size);
  }

  // 1. Try oversized bins (guaranteed fit, but larger).
  size_t oversized_bit = free_sizes.find_first_occupied_after(bit_index);
  if (LIBC_LIKELY(oversized_bit < TOTAL_BITS)) {
    if (LIBC_UNLIKELY(oversized_bit == TOTAL_BITS - 1)) {
      if constexpr (USE_TRIE)
        return find_and_remove_fit_in_trie(size);
      else
        return remove_first_fit_in_list(TOTAL_BITS - 1, size);
    }

    BlockRef block = free_lists[oversized_bit].front();
    free_lists[oversized_bit].pop();
    if (free_lists[oversized_bit].empty())
      free_sizes.mark_vacant(oversized_bit);
    return block;
  }

  // 2. Try exact fit (fallback).
  if (free_sizes.is_occupied(bit_index)) {
    if (BlockRef block = remove_first_fit_in_list(bit_index, size))
      return block;
  }

  return BlockRef();
}

template <typename CONFIG = DefaultFreeStoreConfig>
using TLSFFreeStore = TLSFFreeStoreImpl<CONFIG>;

using FreeStore = TLSFFreeStore<DefaultFreeStoreConfig>;

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FREESTORE_H
