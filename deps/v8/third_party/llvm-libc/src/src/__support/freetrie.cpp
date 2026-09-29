//===-- Implementation for freetrie ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "freetrie.h"

namespace LIBC_NAMESPACE_DECL {

void FreeTrie::remove(Node *node) {
  LIBC_ASSERT(!empty() && "cannot remove from empty trie");
  FreeList list = node;
  list.pop();
  Node *new_node = static_cast<Node *>(list.begin());
  if (!new_node) {
    // The freelist is empty. Replace the subtrie root with an arbitrary leaf.
    // This is legal because there is no relationship between the size of the
    // root and its children.
    Node *leaf = node;
    while (leaf->lower || leaf->upper)
      leaf = leaf->lower ? leaf->lower : leaf->upper;
    if (leaf == node) {
      // If the root is a leaf, then removing it empties the subtrie.
      replace_node(node, nullptr);
      return;
    }

    replace_node(leaf, nullptr);
    new_node = leaf;
  }

  if (!is_head(node))
    return;

  // Copy the trie links to the new head.
  new_node->lower = node->lower;
  new_node->upper = node->upper;
  new_node->parent = node->parent;
  replace_node(node, new_node);
}

void FreeTrie::replace_node(Node *node, Node *new_node) {
  LIBC_ASSERT(is_head(node) && "only head nodes contain trie links");

  if (node->parent) {
    Node *&parent_child =
        node->parent->lower == node ? node->parent->lower : node->parent->upper;
    LIBC_ASSERT(parent_child == node &&
                "no reference to child node found in parent");
    parent_child = new_node;
  } else {
    LIBC_ASSERT(root == node && "non-root node had no parent");
    root = new_node;
  }
  if (node->lower)
    node->lower->parent = new_node;
  if (node->upper)
    node->upper->parent = new_node;
}

} // namespace LIBC_NAMESPACE_DECL
