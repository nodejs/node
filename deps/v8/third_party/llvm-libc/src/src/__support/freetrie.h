//===-- Interface for freetrie --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FREETRIE_H
#define LLVM_LIBC_SRC___SUPPORT_FREETRIE_H

#include "freelist.h"

namespace LIBC_NAMESPACE_DECL {

/// A trie of free lists.
///
/// This is an unusual little data structure originally from Doug Lea's malloc.
/// Finding the best fit from a set of differently-sized free list typically
/// required some kind of ordered map, and these are typically implemented using
/// a self-balancing binary search tree. Those are notorious for having a
/// relatively large number of special cases, while this trie has relatively
/// few, which helps with code size.
///
/// Operations on the trie are logarithmic not on the number of nodes within it,
/// but rather the fixed range of possible sizes that the trie can contain. This
/// means that the data structure would likely actually perform worse than an
/// e.g. red-black tree, but its implementation is still much simpler.
///
/// Each trie node's children subdivide the range of possible sizes into two
/// halves: a lower and an upper. The node itself holds a free list of some size
/// within its range. This makes it possible to summarily replace any node with
/// any leaf within its subtrie, which makes it very straightforward to remove a
/// node. Insertion is also simple; the only real complexity lies with finding
/// the best fit. This can still be done in logarithmic time with only a few
/// cases to consider.
///
/// The trie refers to, but does not own, the Nodes that comprise it.
class FreeTrie {
public:
  /// A trie node that is also a free list. Only the head node of each list is
  /// actually part of the trie. The subtrie contains a continous SizeRange of
  /// free lists. The lower and upper subtrie's contain the lower and upper half
  /// of the subtries range. There is no direct relationship between the size of
  /// this node's free list and the contents of the lower and upper subtries.
  class Node : public FreeList::Node {
    /// The child subtrie covering the lower half of this subtrie's size range.
    /// Undefined if this is not the head of the list.
    Node *lower;
    /// The child subtrie covering the upper half of this subtrie's size range.
    /// Undefined if this is not the head of the list.
    Node *upper;
    /// The parent subtrie. nullptr if this is the root or not the head of the
    /// list.
    Node *parent;

    friend class FreeTrie;
  };

  /// Power-of-two range of sizes covered by a subtrie.
  struct SizeRange {
    size_t min;
    size_t width;

    LIBC_INLINE constexpr SizeRange(size_t min, size_t width)
        : min(min), width(width) {
      LIBC_ASSERT(!(width & (width - 1)) && "width must be a power of two");
    }

    /// @returns The lower half of the size range.
    LIBC_INLINE SizeRange lower() const { return {min, width / 2}; }

    /// @returns The upper half of the size range.
    LIBC_INLINE SizeRange upper() const { return {min + width / 2, width / 2}; }

    /// @returns The largest size in this range.
    LIBC_INLINE size_t max() const { return min + (width - 1); }

    /// @returns Whether the range contains the given size.
    LIBC_INLINE bool contains(size_t size) const {
      return min <= size && size < min + width;
    }
  };

  LIBC_INLINE constexpr FreeTrie() : FreeTrie(SizeRange{0, 0}) {}
  LIBC_INLINE constexpr FreeTrie(SizeRange range) : range(range) {}

  /// Sets the range of possible block sizes. This can only be called when the
  /// trie is empty.
  LIBC_INLINE void set_range(FreeTrie::SizeRange new_range) {
    LIBC_ASSERT(empty() && "cannot change the range of a preexisting trie");
    range = new_range;
  }

  /// @returns Whether the trie contains any blocks.
  LIBC_INLINE bool empty() const { return !root; }

  /// Push a block to the trie.
  void push(Block *block);

  /// Remove a node from this trie node's free list.
  void remove(Node *node);

  /// @returns A smallest node that can allocate the given size; otherwise
  /// nullptr.
  Node *find_best_fit(size_t size);

private:
  /// @returns Whether a node is the head of its containing freelist.
  bool is_head(Node *node) const { return node->parent || node == root; }

  /// Replaces references to one node with another (or nullptr) in all adjacent
  /// parent and child nodes.
  void replace_node(Node *node, Node *new_node);

  Node *root = nullptr;
  SizeRange range;
};

LIBC_INLINE void FreeTrie::push(Block *block) {
  LIBC_ASSERT(block->inner_size_free() >= sizeof(Node) &&
              "block too small to accomodate free trie node");
  size_t size = block->inner_size();
  LIBC_ASSERT(range.contains(size) && "requested size out of trie range");

  // Find the position in the tree to push to.
  Node **cur = &root;
  Node *parent = nullptr;
  SizeRange cur_range = range;
  while (*cur && (*cur)->size() != size) {
    LIBC_ASSERT(cur_range.contains(size) && "requested size out of trie range");
    parent = *cur;
    if (size <= cur_range.lower().max()) {
      cur = &(*cur)->lower;
      cur_range = cur_range.lower();
    } else {
      cur = &(*cur)->upper;
      cur_range = cur_range.upper();
    }
  }

  Node *node = new (block->usable_space()) Node;
  FreeList list = *cur;
  if (list.empty()) {
    node->parent = parent;
    node->lower = node->upper = nullptr;
  } else {
    node->parent = nullptr;
  }
  list.push(node);
  *cur = static_cast<Node *>(list.begin());
}

LIBC_INLINE FreeTrie::Node *FreeTrie::find_best_fit(size_t size) {
  if (empty() || range.max() < size)
    return nullptr;

  Node *cur = root;
  SizeRange cur_range = range;
  Node *best_fit = nullptr;
  Node *deferred_upper_trie = nullptr;
  FreeTrie::SizeRange deferred_upper_range{0, 0};

  while (true) {
    LIBC_ASSERT(cur_range.contains(cur->size()) &&
                "trie node size out of range");
    LIBC_ASSERT(cur_range.max() >= size &&
                "range could not fit requested size");
    LIBC_ASSERT((!best_fit || cur_range.min < best_fit->size()) &&
                "range could not contain a best fit");

    // If the current node is an exact fit, it is a best fit.
    if (cur->size() == size)
      return cur;

    if (cur->size() > size && (!best_fit || cur->size() < best_fit->size())) {
      // The current node is a better fit.
      best_fit = cur;

      // If there is a deferred upper subtrie, then the current node is
      // somewhere in its lower sibling subtrie. That means that the new best
      // fit is better than the best fit in the deferred subtrie.
      LIBC_ASSERT(
          (!deferred_upper_trie ||
           deferred_upper_range.min > best_fit->size()) &&
          "deferred upper subtrie should be outclassed by new best fit");
      deferred_upper_trie = nullptr;
    }

    // Determine which subtries might contain the best fit.
    bool lower_impossible = !cur->lower || cur_range.lower().max() < size;
    bool upper_impossible =
        !cur->upper ||
        // If every node in the lower trie fits
        (!lower_impossible && cur_range.min >= size) ||
        // If every node in the upper trie is worse than the current best
        (best_fit && cur_range.upper().min >= best_fit->size());

    if (lower_impossible && upper_impossible) {
      if (!deferred_upper_trie)
        return best_fit;
      // Scan the deferred upper subtrie and consider whether any element within
      // provides a better fit.
      //
      // This can only ever be reached once. In a deferred upper subtrie, every
      // node fits, so the higher of two subtries can never contain a best fit.
      cur = deferred_upper_trie;
      cur_range = deferred_upper_range;
      deferred_upper_trie = nullptr;
      continue;
    }

    if (lower_impossible) {
      cur = cur->upper;
      cur_range = cur_range.upper();
    } else if (upper_impossible) {
      cur = cur->lower;
      cur_range = cur_range.lower();
    } else {
      // Both subtries might contain a better fit. Any fit in the lower subtrie
      // is better than the any fit in the upper subtrie, so scan the lower
      // and return to the upper only if no better fits were found. (Any better
      // fit found clears the deferred upper subtrie.)
      LIBC_ASSERT((!deferred_upper_trie ||
                   cur_range.upper().max() < deferred_upper_range.min) &&
                  "old deferred upper subtrie should be outclassed by new");
      deferred_upper_trie = cur->upper;
      deferred_upper_range = cur_range.upper();
      cur = cur->lower;
      cur_range = cur_range.lower();
    }
  }
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FREETRIE_H
