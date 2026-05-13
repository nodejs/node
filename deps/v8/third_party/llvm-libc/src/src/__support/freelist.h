//===-- Interface for freelist --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FREELIST_H
#define LLVM_LIBC_SRC___SUPPORT_FREELIST_H

#include "block.h"

namespace LIBC_NAMESPACE_DECL {

/// A circularly-linked FIFO list storing free Blocks. All Blocks on a list
/// are the same size. The blocks are referenced by Nodes in the list; the list
/// refers to these, but it does not own them.
///
/// Allocating free blocks in FIFO order maximizes the amount of time before a
/// free block is reused. This in turn maximizes the number of opportunities for
/// it to be coalesced with an adjacent block, which tends to reduce heap
/// fragmentation.
class FreeList {
public:
  class Node {
  public:
    /// @returns The block containing this node.
    LIBC_INLINE const Block *block() const {
      return Block::from_usable_space(this);
    }

    /// @returns The block containing this node.
    LIBC_INLINE Block *block() { return Block::from_usable_space(this); }

    /// @returns The inner size of blocks in the list containing this node.
    LIBC_INLINE size_t size() const { return block()->inner_size(); }

  private:
    // Circularly linked pointers to adjacent nodes.
    Node *prev;
    Node *next;
    friend class FreeList;
  };

  LIBC_INLINE constexpr FreeList() : FreeList(nullptr) {}
  LIBC_INLINE constexpr FreeList(Node *begin) : begin_(begin) {}

  LIBC_INLINE bool empty() const { return !begin_; }

  /// @returns The inner size of blocks in the list.
  LIBC_INLINE size_t size() const {
    LIBC_ASSERT(begin_ && "empty lists have no size");
    return begin_->size();
  }

  /// @returns The first node in the list.
  LIBC_INLINE Node *begin() { return begin_; }

  /// @returns The first block in the list.
  LIBC_INLINE Block *front() { return begin_->block(); }

  /// Push a block to the back of the list.
  /// The block must be large enough to contain a node.
  LIBC_INLINE void push(Block *block) {
    LIBC_ASSERT(!block->used() &&
                "only free blocks can be placed on free lists");
    LIBC_ASSERT(block->inner_size_free() >= sizeof(FreeList) &&
                "block too small to accomodate free list node");
    push(new (block->usable_space()) Node);
  }

  /// Push an already-constructed node to the back of the list.
  /// This allows pushing derived node types with additional data.
  void push(Node *node);

  /// Pop the first node from the list.
  LIBC_INLINE void pop() { remove(begin_); }

  /// Remove an arbitrary node from the list.
  void remove(Node *node);

private:
  Node *begin_;
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FREELIST_H
