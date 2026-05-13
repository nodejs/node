//===-- Interface for freestore ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FREESTORE_H
#define LLVM_LIBC_SRC___SUPPORT_FREESTORE_H

#include "freetrie.h"

namespace LIBC_NAMESPACE_DECL {

/// A best-fit store of variously-sized free blocks. Blocks can be inserted and
/// removed in logarithmic time.
class FreeStore {
public:
  FreeStore() = default;
  FreeStore(const FreeStore &other) = delete;
  FreeStore &operator=(const FreeStore &other) = delete;

  /// Sets the range of possible block sizes. This can only be called when the
  /// trie is empty.
  LIBC_INLINE void set_range(FreeTrie::SizeRange range) {
    large_trie.set_range(range);
  }

  /// Insert a free block. If the block is too small to be tracked, nothing
  /// happens.
  void insert(Block *block);

  /// Remove a free block. If the block is too small to be tracked, nothing
  /// happens.
  void remove(Block *block);

  /// Remove a best-fit free block that can contain the given size when
  /// allocated. Returns nullptr if there is no such block.
  Block *remove_best_fit(size_t size);

private:
  static constexpr size_t MIN_OUTER_SIZE =
      align_up(sizeof(Block) + sizeof(FreeList::Node), Block::MIN_ALIGN);
  static constexpr size_t MIN_LARGE_OUTER_SIZE =
      align_up(sizeof(Block) + sizeof(FreeTrie::Node), Block::MIN_ALIGN);
  static constexpr size_t NUM_SMALL_SIZES =
      (MIN_LARGE_OUTER_SIZE - MIN_OUTER_SIZE) / Block::MIN_ALIGN;

  LIBC_INLINE static bool too_small(Block *block) {
    return block->outer_size() < MIN_OUTER_SIZE;
  }
  LIBC_INLINE static bool is_small(Block *block) {
    return block->outer_size() < MIN_LARGE_OUTER_SIZE;
  }

  FreeList &small_list(Block *block);
  FreeList *find_best_small_fit(size_t size);

  cpp::array<FreeList, NUM_SMALL_SIZES> small_lists;
  FreeTrie large_trie;
};

LIBC_INLINE void FreeStore::insert(Block *block) {
  if (too_small(block))
    return;
  if (is_small(block))
    small_list(block).push(block);
  else
    large_trie.push(block);
}

LIBC_INLINE void FreeStore::remove(Block *block) {
  if (too_small(block))
    return;
  if (is_small(block)) {
    small_list(block).remove(
        reinterpret_cast<FreeList::Node *>(block->usable_space()));
  } else {
    large_trie.remove(
        reinterpret_cast<FreeTrie::Node *>(block->usable_space()));
  }
}

LIBC_INLINE Block *FreeStore::remove_best_fit(size_t size) {
  if (FreeList *list = find_best_small_fit(size)) {
    Block *block = list->front();
    list->pop();
    return block;
  }
  if (FreeTrie::Node *best_fit = large_trie.find_best_fit(size)) {
    Block *block = best_fit->block();
    large_trie.remove(best_fit);
    return block;
  }
  return nullptr;
}

LIBC_INLINE FreeList &FreeStore::small_list(Block *block) {
  LIBC_ASSERT(is_small(block) && "only legal for small blocks");
  return small_lists[(block->outer_size() - MIN_OUTER_SIZE) / Block::MIN_ALIGN];
}

LIBC_INLINE FreeList *FreeStore::find_best_small_fit(size_t size) {
  for (FreeList &list : small_lists)
    if (!list.empty() && list.size() >= size)
      return &list;
  return nullptr;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FREESTORE_H
