//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Interface for freelist_heap.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FREELIST_HEAP_H
#define LLVM_LIBC_SRC___SUPPORT_FREELIST_HEAP_H

#include <stddef.h>

#include "block.h"
#include "freestore.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/math_extras.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/memory_utils/inline_memset.h"

namespace LIBC_NAMESPACE_DECL {

extern "C" cpp::byte _end;
extern "C" cpp::byte __llvm_libc_heap_limit;

using cpp::optional;
using cpp::span;

LIBC_INLINE constexpr bool IsPow2(size_t x) { return x && (x & (x - 1)) == 0; }

class FreeListHeap {
public:
  /// The heap keeps its free blocks in NUM_FREE_STORES stores and rotates
  /// between them: allocations are served from the active store, while frees
  /// go to the next one, quarantining the memory there. Only once the active
  /// store cannot satisfy a request does rotate() make the quarantined memory
  /// available again. Delaying reuse this way makes use-after-free bugs much
  /// more likely to be caught, e.g. by a sanitizer.
  ///
  /// A request the active store cannot serve thus ends the quarantine period
  /// early, whether or not it succeeds afterwards.
  ///
  /// With a single store (the default) the quarantine store is the active
  /// store, no rotation ever happens, and this degenerates into the usual
  /// immediate-reuse behavior.
  static constexpr size_t NUM_FREE_STORES = BlockRef::NUM_FREE_STORES;

  constexpr FreeListHeap() : begin(&_end), end(&__llvm_libc_heap_limit) {}

  constexpr FreeListHeap(span<cpp::byte> region)
      : begin(region.begin()), end(region.end()) {}

  void *allocate(size_t size);
  void *aligned_allocate(size_t alignment, size_t size);
  // NOTE: All pointers passed to free must come from one of the other
  // allocation functions: `allocate`, `aligned_allocate`, `realloc`, `calloc`.
  void free(void *ptr);
  void *realloc(void *ptr, size_t size);
  void *calloc(size_t num, size_t size);
  size_t allocation_size(const void *ptr) const;
  LIBC_INLINE void integrity_check() const {
    for (const FreeStore &store : free_stores)
      store.integrity_check();
  }

  cpp::span<cpp::byte> region() const { return {begin, end}; }

private:
  void init();

  void *allocate_impl(size_t alignment, size_t size);

  span<cpp::byte> block_to_span(BlockRef block) {
    return span<cpp::byte>(block.usable_space(), block.inner_size());
  }

  bool shrink_in_place(BlockRef block, size_t size);

  bool is_valid_ptr(const void *ptr) const { return ptr >= begin && ptr < end; }

  /// The store that allocations are served from.
  LIBC_INLINE FreeStore &active_free_store() { return free_stores[active]; }

  /// @returns The index of the store that receives newly freed blocks. With a
  /// single free store this is the active store itself, so freed memory is
  /// immediately available again.
  LIBC_INLINE size_t quarantine_store_index() const {
    return (active + 1) % NUM_FREE_STORES;
  }

  /// Whether the free block `neighbor` can be merged into a block owned by
  /// `store_index`. Merging across stores would hand quarantined memory back
  /// out through the active store, so only blocks of the same store merge,
  /// plus blocks too small for any store to track (see FreeStore::too_small),
  /// which are owned by no store.
  LIBC_INLINE static bool can_merge(BlockRef neighbor, size_t store_index) {
    return FreeStore::too_small(neighbor) ||
           neighbor.next().prev_free_store_index() ==
               static_cast<int>(store_index);
  }

  /// Inserts `block` into store `store_index`, coalescing it with the
  /// neighbors that can be merged into that store. The block may be in use or
  /// free in another store.
  LIBC_INLINE void insert(BlockRef block, size_t store_index) {
    block.mark_free(store_index);

    // Absorbing an untracked neighbor may expose another mergeable one beyond
    // it, so keep going.
    for (BlockRef prev = block.prev_free();
         prev && can_merge(prev, store_index); prev = block.prev_free()) {
      free_stores[store_index].remove(prev);
      block = prev;
      block.merge_next();
    }

    for (BlockRef next = block.next();
         !next.used() && can_merge(next, store_index); next = block.next()) {
      free_stores[store_index].remove(next);
      block.merge_next();
    }

    // Merging moved the block boundaries, and an absorbed block may have been
    // owned by another store, so record the ownership of the result again.
    block.mark_free(store_index);
    free_stores[store_index].insert(block);
  }

  /// Ends the current quarantine period: the store that has been collecting
  /// freed blocks becomes the active one, and whatever is left in the
  /// previously active store is migrated (and coalesced) into it, so that the
  /// newly active store holds all the free memory of the heap. Returns false
  /// without doing anything if nothing is quarantined.
  LIBC_INLINE bool rotate() {
    if (NUM_FREE_STORES < 2 || free_stores[quarantine_store_index()].empty())
      return false;

    size_t prev_active = active;
    active = quarantine_store_index();
    while (BlockRef block = free_stores[prev_active].remove_any())
      insert(block, active);
    return true;
  }

  cpp::byte *begin;
  cpp::byte *end;
  bool is_initialized = false;
  FreeStore free_stores[NUM_FREE_STORES];
  size_t active = 0;
};

template <size_t BUFF_SIZE> class FreeListHeapBuffer : public FreeListHeap {
public:
  LIBC_INLINE constexpr FreeListHeapBuffer() : FreeListHeap{buffer}, buffer{} {}

private:
  cpp::byte buffer[BUFF_SIZE];
};

LIBC_INLINE void FreeListHeap::init() {
  LIBC_ASSERT(!is_initialized && "duplicate initialization");
  auto result = BlockRef::init(region());
  BlockRef block = *result;
  for (FreeStore &store : free_stores)
    store.set_range({0, cpp::bit_ceil(block.inner_size())});
  free_stores[active].insert(block);
  is_initialized = true;
}

LIBC_INLINE void *FreeListHeap::allocate_impl(size_t alignment, size_t size) {
  if (size == 0)
    return nullptr;

  if (!is_initialized)
    init();

  size_t request_size = BlockRef::min_size_for_allocation(alignment, size);
  // Don't disturb the quarantine for a request the heap could never serve.
  if (!request_size || request_size > region().size())
    return nullptr;

  BlockRef block = active_free_store().remove_best_fit(request_size);
  // The active store is out of memory; make the quarantined blocks available
  // again and retry.
  if (!block && rotate())
    block = active_free_store().remove_best_fit(request_size);
  if (!block)
    return nullptr;

  auto block_info = BlockRef::allocate(block, alignment, size);
  block_info.block.mark_used();
  // The leftovers of the block were never handed out, so they stay in the
  // active store rather than being quarantined.
  if (block_info.next)
    insert(block_info.next, active);
  if (block_info.prev)
    insert(block_info.prev, active);
  return block_info.block.usable_space();
}

LIBC_INLINE void *FreeListHeap::allocate(size_t size) {
  return allocate_impl(BlockRef::MIN_ALIGN, size);
}

LIBC_INLINE void *FreeListHeap::aligned_allocate(size_t alignment,
                                                 size_t size) {
  // The alignment must be an integral power of two.
  if (!IsPow2(alignment))
    return nullptr;

  // The size parameter must be an integral multiple of alignment.
  if (size % alignment != 0)
    return nullptr;

  // The minimum alignment supported by BlockRef is MIN_ALIGN.
  alignment = cpp::max(alignment, BlockRef::MIN_ALIGN);

  return allocate_impl(alignment, size);
}

LIBC_INLINE void FreeListHeap::free(void *ptr) {
  if (ptr == nullptr)
    return;

  cpp::byte *bytes = static_cast<cpp::byte *>(ptr);

  LIBC_ASSERT(is_valid_ptr(bytes) && "Invalid pointer");

  BlockRef block = BlockRef::from_usable_space(bytes);
  LIBC_ASSERT(block.next() && "sentinel last block cannot be freed");
  LIBC_ASSERT(block.used() && "double free");
  insert(block, quarantine_store_index());
}

LIBC_INLINE size_t FreeListHeap::allocation_size(const void *ptr) const {
  if (!is_valid_ptr(ptr))
    return 0;
  BlockRef block = BlockRef::from_usable_space(ptr);
  if (!block.used())
    return 0;
  return block.inner_size();
}

LIBC_INLINE bool FreeListHeap::shrink_in_place(BlockRef block, size_t size) {
  size_t min_outer_size = BlockRef::outer_size(cpp::max(size, sizeof(size_t)));
  uintptr_t next_block_start = BlockRef::next_possible_block_start(
      block.addr() + min_outer_size, BlockRef::MIN_ALIGN);
  size_t new_outer_size = next_block_start - block.addr();
  if (block.outer_size() >= new_outer_size) {
    optional<BlockRef> next = block.split(size);
    // register the new block on successful split
    if (next.has_value()) {
      BlockRef next_block = *next;
      // Since the original block was not the last block (the sentinel last
      // block is never split), the split-off remainder block `next_block` is
      // also not the last block. Thus, its next block is guaranteed to be
      // non-null.
      LIBC_ASSERT(next_block.next() && "right block must be non-null");
      // The remainder is memory the caller has given up, so quarantine it.
      insert(next_block, quarantine_store_index());
    }
    return true;
  }
  return false;
}

// Follows constract of the C standard realloc() function
// If ptr is free'd, will return nullptr.
LIBC_INLINE void *FreeListHeap::realloc(void *ptr, size_t size) {
  if (size == 0) {
    free(ptr);
    return nullptr;
  }

  // If the pointer is nullptr, allocate a new memory.
  if (ptr == nullptr)
    return allocate(size);

  cpp::byte *bytes = static_cast<cpp::byte *>(ptr);

  if (!is_valid_ptr(bytes))
    return nullptr;

  BlockRef block = BlockRef::from_usable_space(bytes);
  if (!block.used())
    return nullptr;
  size_t old_size = block.inner_size();

  if (old_size >= size) {
    shrink_in_place(block, size);
    return ptr;
  }

  void *new_ptr = allocate(size);
  // Don't invalidate ptr if allocate(size) fails to initilize the memory.
  if (new_ptr == nullptr)
    return nullptr;
  LIBC_NAMESPACE::inline_memcpy(new_ptr, ptr, old_size);

  free(ptr);
  return new_ptr;
}

LIBC_INLINE void *FreeListHeap::calloc(size_t num, size_t size) {
  size_t bytes;
  if (__builtin_mul_overflow(num, size, &bytes))
    return nullptr;
  void *ptr = allocate(bytes);
  if (ptr != nullptr)
    LIBC_NAMESPACE::inline_memset(ptr, 0, bytes);
  return ptr;
}

extern FreeListHeap *freelist_heap;

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FREELIST_HEAP_H
