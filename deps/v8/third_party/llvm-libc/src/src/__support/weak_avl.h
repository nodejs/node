//===-- Implementation header for weak AVL tree -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_WEAK_AVL_H
#define LLVM_LIBC_SRC___SUPPORT_WEAK_AVL_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/new.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/utility/move.h"
#include "src/__support/alloc-checker.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

// A general self-balancing binary search tree where the node pointer can
// be used as stable handles to the stored values.
//
// The self-balancing strategy is the Weak AVL (WAVL) tree, based on the
// following foundational references:
// 1. https://maskray.me/blog/2025-12-14-weak-avl-tree
// 2. https://reviews.freebsd.org/D25480
// 3. https://ics.uci.edu/~goodrich/teach/cs165/notes/WeakAVLTrees.pdf
// 4. https://dl.acm.org/doi/10.1145/2689412 (Rank-Balanced Trees)
//
// WAVL trees belong to the rank-balanced binary search tree framework
// (reference 4), alongside AVL and Red-Black trees.
//
// Key Properties of WAVL Trees:
// 1. Relationship to Red-Black Trees: A WAVL tree can always be colored as a
//    Red-Black tree.
// 2. Relationship to AVL Trees: An AVL tree meets all the requirements of a
//    WAVL tree. Insertion-only WAVL trees maintain the same structure as AVL
//    trees.
//
// Rank-Based Balancing:
// In rank-balanced trees, each node is assigned a rank (conceptually similar
// to height). The rank difference between a parent and its child is
// strictly enforced to be either **1** or **2**.
//
// - **AVL Trees:** Rank is equivalent to height. The strict condition is that
//   there are no 2-2 nodes (a parent with rank difference 2 to both children).
// - **WAVL Trees:** The no 2-2 node rule is relaxed for internal nodes during
//   the deletion fixup process, making WAVL trees less strictly balanced than
//   AVL trees but easier to maintain than Red-Black trees.
//
// Balancing Mechanics (Promotion/Demotion):
// - **Null nodes** are considered to have rank -1.
// - **External/leaf nodes** have rank 0.
// - **Insertion:** Inserting a node may create a situation where a parent and
//   child have the same rank (difference 0). This is fixed by **promoting**
//   the rank of the parent and propagating the fix upwards using at most two
//   rotations (trinode fixup).
// - **Deletion:** Deleting a node may result in a parent being 3 ranks higher
//   than a child (difference 3). This is fixed by **demoting** the parent's
//   rank and propagating the fix upwards.
//
// Implementation Detail:
// The rank is **implicitly** maintained. We never store the full rank. Instead,
// a 2-bit tag is used on each node to record the rank difference to each child:
// - Bit cleared (0) -> Rank difference is **1**.
// - Bit set (1)     -> Rank difference is **2**.
template <typename T> class WeakAVLNode {
  // Data
  T data;

  // Parent pointer
  WeakAVLNode *parent;

  // Children pointers
  WeakAVLNode *children[2];

  // Flags
  unsigned char left_rank_diff_2 : 1;
  unsigned char right_rank_diff_2 : 1;

  LIBC_INLINE bool is_leaf() const {
    return (children[0] == nullptr) && (children[1] == nullptr);
  }

  LIBC_INLINE void toggle_rank_diff_2(bool is_right) {
    if (is_right)
      right_rank_diff_2 ^= 1;
    else
      left_rank_diff_2 ^= 1;
  }

  LIBC_INLINE bool both_flags_set() const {
    return left_rank_diff_2 && right_rank_diff_2;
  }

  LIBC_INLINE bool any_flag_set() const {
    return left_rank_diff_2 || right_rank_diff_2;
  }

  LIBC_INLINE void clear_flags() {
    left_rank_diff_2 = 0;
    right_rank_diff_2 = 0;
  }

  LIBC_INLINE void set_both_flags() {
    left_rank_diff_2 = 1;
    right_rank_diff_2 = 1;
  }

  LIBC_INLINE WeakAVLNode(T data)
      : data(cpp::move(data)), parent(nullptr), children{nullptr, nullptr},
        left_rank_diff_2(0), right_rank_diff_2(0) {}

  LIBC_INLINE static WeakAVLNode *create(T value) {
    AllocChecker ac;
    WeakAVLNode *res = new (ac) WeakAVLNode(value);
    if (ac)
      return res;
    return nullptr;
  }

  // Unlink a node from tree. The corresponding flag is not updated. The node is
  // not deleted and its pointers are not cleared.
  // FixupSite is the lowest surviving node from which rank/flag invariants may
  // be violated.
  // Our tree requires value to stay in their node to maintain stable addresses.
  // This complicates the unlink operation as the successor transplanting needs
  // to update all the pointers and flags.
  struct FixupSite {
    WeakAVLNode *parent;
    bool is_right;
  };
  LIBC_INLINE static FixupSite unlink(WeakAVLNode *&root, WeakAVLNode *node) {
    bool has_left = node->children[0] != nullptr;
    bool has_right = node->children[1] != nullptr;

    // Case 0: no children
    if (!has_left && !has_right) {
      if (!node->parent) {
        root = nullptr;
        return {nullptr, false};
      }
      FixupSite site = {node->parent, node->parent->children[1] == node};
      site.parent->children[site.is_right] = nullptr;
      return site;
    }

    // Case 1: one child
    if (has_left != has_right) {
      WeakAVLNode *child = node->children[has_right];
      if (!node->parent) {
        root = child;
        child->parent = nullptr;
        return {nullptr, false};
      }
      FixupSite site = {node->parent, node->parent->children[1] == node};
      site.parent->children[site.is_right] = child;
      child->parent = site.parent;
      return site;
    }

    // Case 2: two children: replace by successor (leftmost in right subtree)
    WeakAVLNode *succ = node->children[1];
    while (succ->children[0])
      succ = succ->children[0];

    WeakAVLNode *succ_parent = succ->parent;
    // succ and node may be adjacent to each other, so we
    // still need to check the exact direction of the successor.
    bool succ_was_right = succ_parent->children[1] == succ;
    WeakAVLNode *succ_rchild = succ->children[1];

    // 1) Splice successor out of its old position (flags intentionally
    // unchanged)
    FixupSite site = {succ_parent, succ_was_right};
    succ_parent->children[succ_was_right] = succ_rchild;
    if (succ_rchild)
      succ_rchild->parent = succ_parent;

    // 2) Transplant successor into node's position
    succ->parent = node->parent;
    succ->left_rank_diff_2 = node->left_rank_diff_2;
    succ->right_rank_diff_2 = node->right_rank_diff_2;

    succ->children[0] = node->children[0];
    succ->children[1] = node->children[1];
    if (succ->children[0])
      succ->children[0]->parent = succ;
    if (succ->children[1])
      succ->children[1]->parent = succ;

    if (succ->parent) {
      bool node_was_right = succ->parent->children[1] == node;
      succ->parent->children[node_was_right] = succ;
    } else {
      root = succ;
    }

    // 3) If the physical removal was under `node`, fixup parent must be the
    //    successor (since `node` is deleted and successor now occupies that
    //    spot).
    if (site.parent == node)
      site.parent = succ;

    return site;
  }

public:
  using OptionalNodePtr = cpp::optional<WeakAVLNode *>;

  LIBC_INLINE const WeakAVLNode *get_left() const { return children[0]; }
  LIBC_INLINE const WeakAVLNode *get_right() const { return children[1]; }
  LIBC_INLINE const WeakAVLNode *get_parent() const { return parent; }
  LIBC_INLINE const T &get_data() const { return data; }
  LIBC_INLINE bool has_rank_diff_2(bool is_right) const {
    return is_right ? right_rank_diff_2 : left_rank_diff_2;
  }

  // Destroy the subtree rooted at node
  LIBC_INLINE static void destroy(WeakAVLNode *node) {
    if (!node)
      return;
    destroy(node->children[0]);
    destroy(node->children[1]);
    delete node;
  }

  // Destroy the subtree rooted at node with finalizer
  template <typename Finalizer>
  LIBC_INLINE static void destroy(WeakAVLNode *node, Finalizer finalizer) {
    if (!node)
      return;
    destroy(node->children[0], finalizer);
    destroy(node->children[1], finalizer);
    finalizer(node->data);
    delete node;
  }
  // Rotate the subtree rooted at node in the given direction.
  //
  // Illustration for is_right = true (Left Rotation):
  //
  //          (Node)                       (Pivot)
  //          /    \                       /     \
  //         A   (Pivot)       =>       (Node)    C
  //             /     \                /    \
  //            B       C              A      B
  //
  LIBC_INLINE static WeakAVLNode *rotate(WeakAVLNode *&root, WeakAVLNode *node,
                                         bool is_right) {
    WeakAVLNode *pivot = node->children[is_right];
    // Handover pivot's child
    WeakAVLNode *grandchild = pivot->children[!is_right];
    node->children[is_right] = grandchild;
    if (grandchild)
      grandchild->parent = node;
    pivot->parent = node->parent;
    // Pivot becomes the new root of the subtree
    if (!node->parent) {
      root = pivot;
    } else {
      bool node_is_right = node->parent->children[1] == node;
      node->parent->children[node_is_right] = pivot;
    }
    pivot->children[!is_right] = node;
    node->parent = pivot;
    return pivot;
  }

  // Find data in the subtree rooted at root. If not found, returns
  // OptionalNode. `Compare` returns integer values for ternary comparison.
  // Unlike other interfaces, `find` does not modify the tree; hence we pass
  // the `root` by value.
  // It is assumed that the order returned by the comparator is consistent
  // on each call.
  template <typename Compare>
  LIBC_INLINE static OptionalNodePtr find(WeakAVLNode *root, T data,
                                          Compare comp) {
    WeakAVLNode *cursor = root;
    while (cursor != nullptr) {
      int comp_result = comp(cursor->data, data);
      if (comp_result == 0)
        return cursor; // Node found
      bool is_right = comp_result < 0;
      cursor = cursor->children[is_right];
    }
    return cpp::nullopt;
  }
  // Insert data into the subtree rooted at root.
  // Returns the node if insertion is successful or the node exists in
  // the tree.
  // Returns cpp::nullopt if memory allocation fails.
  // `Compare` returns integer values for ternary comparison.
  // It is assumed that the order returned by the comparator is consistent
  // on each call.
  template <typename Compare>
  LIBC_INLINE static OptionalNodePtr find_or_insert(WeakAVLNode *&root, T data,
                                                    Compare comp) {
    WeakAVLNode *parent = nullptr, *cursor = root;
    bool is_right = false;
    while (cursor != nullptr) {
      parent = cursor;
      int comp_result = comp(parent->data, data);
      if (comp_result == 0)
        return parent; // Node already exists
      is_right = comp_result < 0;
      cursor = cursor->children[is_right];
    }
    WeakAVLNode *allocated = create(cpp::move(data));
    if (!allocated)
      return cpp::nullopt;
    WeakAVLNode *node = allocated;
    node->parent = parent;

    // Case 0: inserting into an empty tree
    if (!parent) {
      root = node; // Tree was empty
      return node;
    }

    parent->children[is_right] = node;
    // Rebalance process
    // Case 1: both node and its sibling have rank-difference 1. So after the
    // insertion, the node is at the same level as the parent. Promoting parent
    // will fix the conflict of the trinodes but we may need to continue on
    // parent.
    //
    //         (GP)                       (GP)
    //          |         Promote          |   x - 1
    //          | x        ----->         (P)
    //      0   |         /           1  /   \
    // (N) --- (P)    ----             (N)    \ 2
    //            \  1                         \
    //             (S)                          (S)
    while (parent && !parent->any_flag_set()) {
      parent->toggle_rank_diff_2(!is_right);
      node = parent;
      parent = node->parent;
      if (parent)
        is_right = (parent->children[1] == node);
    }
    // We finish if node has reaches the root -- otherwise, we end up with
    // two more cases.
    if (!parent)
      return allocated;

    // Case 2: parent does not need to be promoted as node is lower
    // than the parent by 2 ranks.
    //      (P)                       (P)
    //     /  \                      /  \
    //    2    1           =>       1    1
    //   /      \                  /      \
    // (N)       (*)             (N)       (*)
    if (parent->has_rank_diff_2(is_right)) {
      parent->toggle_rank_diff_2(is_right);
      return allocated;
    }

    // At this point, we know there is a violation but one-step fix is possible.
    LIBC_ASSERT(!node->both_flags_set() &&
                "there should be no 2-2 node along the insertion fixup path");

    LIBC_ASSERT((node == allocated || node->any_flag_set()) &&
                "Internal node must have a child with rank-difference 2, "
                "otherwise it should have already been handled.");

    // Case 3: node's sibling has rank-difference 2. And node has a 1-node
    // along the same direction. We can do a single rotation to fix the
    // trinode.
    //                   (GP)                            (GP)
    //               0    |   X      Rotate               |
    //         (N) ----- (P)           =>                (N)
    //     1  /   \  2      \  2                      1  /  \ 1
    //      (C1)   \         \                        (C1)   (P)
    //             (C2)       (S)                         1 /  \ 1
    //                                                    (C2)  (S)
    if (node->has_rank_diff_2(!is_right)) {
      WeakAVLNode *new_subroot = rotate(root, parent, is_right);
      new_subroot->clear_flags();
      parent->clear_flags();
      return allocated;
    }
    // Case 4: node's sibling has rank-difference 2. And node has a 1-node
    // along the opposite direction. We need a double rotation to fix the
    // trinode.
    //                   (GP)                            (GP)
    //               0    |   X      Zig-Zag              |      X
    //         (N) ----- (P)           =>                (C1)
    //     2  /   \  1      \  2                      1  /  \ 1
    //       /    (C1)       \                        (N)    (P)
    //     (C2) L /  \ R      (S)                  1 / \ L R / \ 1
    //          (A)  (B)                           (C2) (A)(B) (S)
    // (mirrored)
    //         (GP)                                      (GP)
    //        X | 0                Zig-Zag                |      X
    //         (P) ----- (N)           =>                (C1)
    //    2  /         1 / \ 2                        1  /  \ 1
    //      /         (C1)  \                         (P)    (N)
    //    (S)       L /  \ R (C2)                   1 / \ L R / \ 1
    //              (A)  (B)                        (S)(A)  (B)(C2)

    WeakAVLNode *subroot1 = rotate(root, node, !is_right); // First rotation
    [[maybe_unused]] WeakAVLNode *subroot2 =
        rotate(root, parent, is_right); // Second rotation
    LIBC_ASSERT(subroot1 == subroot2 &&
                "Subroots after double rotation should be the same");
    bool subroot_left_diff_2 = subroot1->left_rank_diff_2;
    bool subroot_right_diff_2 = subroot1->right_rank_diff_2;
    node->clear_flags();
    parent->clear_flags();
    subroot1->clear_flags();
    // Select destinations
    WeakAVLNode *dst_left = is_right ? parent : node;
    WeakAVLNode *dst_right = is_right ? node : parent;
    // Masked toggles
    if (subroot_left_diff_2)
      dst_left->toggle_rank_diff_2(true);

    if (subroot_right_diff_2)
      dst_right->toggle_rank_diff_2(false);
    return allocated;
  }

  // Erase the node from the tree rooted at root.
  LIBC_INLINE static void erase(WeakAVLNode *&root, WeakAVLNode *node) {
    // Unlink the node from the tree
    auto [cursor, is_right] = unlink(root, node);
    delete node;
    WeakAVLNode *sibling = nullptr;
    while (cursor) {
      // Case 0. cursor previously had rank-difference 1 on the side of the
      // deleted node. We can simply update the rank-difference and stop.
      // Notice that this step may create 2-2 nodes, thus deviate from "strong"
      // AVL tree.
      //
      //          (C)                 (C)
      //       X /   \ 1     =>    X /   \
      //       (*)   (D)           (*)    \ 2
      //                                   (D)
      if (!cursor->has_rank_diff_2(is_right)) {
        cursor->toggle_rank_diff_2(is_right);
        // If we created a 2-2 leaf, we must demote it and continue.
        // Otherwise, we are done as the internal node becomes a 2-2 node and
        // there is no further violation upwards.
        if (!cursor->both_flags_set() || !cursor->is_leaf())
          return;
        // Clear flags for demotion.
        cursor->clear_flags();
      }

      // Case 1. cursor previously had rank-difference 2 on the side of the
      // deleted node. Now it has rank-difference 3, which violates the
      // weak-AVL property. We found that we have a sibling with rank-difference
      // 2, so we can demote cursor and continue upwards.
      //
      //          (P)                 (P)
      //           |   X               |   (X + 1)
      //          (C)                  |
      //         /   \      =>        (C)
      //     2  /     \            1  / \
      //      (*)      \ 3         (*)   \ 2
      //               (D)                (D)
      else if (cursor->has_rank_diff_2(!is_right))
        cursor->toggle_rank_diff_2(!is_right);

      // Case 2. continue from Case 1; but the sibling has rank-difference 1.
      // However, we found that the sibling is a 2-2 node. We demote both
      // sibling and cursor, and continue upwards. We break for other cases if
      // sibling cannot be demoted.
      //
      //          (P)                 (P)
      //           |   X               |   (X + 1)
      //          (C)                  |
      //      1  /   \      =>        (C)
      //       (S)    \            1  / \
      //      /  \     \ 3         (S)   \ 2
      //   2 /    \ 2   (D)     1 /  \ 1  (D)
      //   (*)    (*)           (*)  (*)
      else {
        sibling = cursor->children[!is_right];
        LIBC_ASSERT(sibling && "rank-difference 1 sibling cannot be empty");
        if (sibling->both_flags_set())
          sibling->clear_flags();
        else
          break;
      }

      // Update cursor to move upwards
      if (cursor->parent)
        is_right = (cursor->parent->children[1] == cursor);
      cursor = cursor->parent;
    }

    // Either cursor is nullptr (we reached the root), or sibling has
    // rank-difference 1.
    if (!cursor)
      return;
    LIBC_ASSERT(sibling && "rank-difference 1 sibling must exist");
    bool sibling_is_right = !is_right; // Rename for clarity

    // Case 3. continue from Case 2; but the sibling cannot be demoted.
    // Sibling has a node T along the same direction with rank-difference 1.
    //
    //          (P)                             (P)
    //           | X                             | X
    //          (C)                             (S)
    //      1  /   \          Rotate         2 /   \ 1
    //       (S)    \           =>            /    (C)
    //    1  / \ Y   \ 3                    (T)   Y / \ 2
    //    (T)   \     (D)                        (*)   \
    //           (*)                                    (D)
    if (!sibling->has_rank_diff_2(sibling_is_right)) {
      WeakAVLNode *new_subroot = rotate(root, cursor, sibling_is_right);
      LIBC_ASSERT(new_subroot == sibling &&
                  "sibling should become the subtree root");
      // Update flags
      bool sibling_alter_child_has_rank_diff_2 =
          new_subroot->has_rank_diff_2(!sibling_is_right);
      new_subroot->clear_flags();
      new_subroot->toggle_rank_diff_2(sibling_is_right);

      // Cursor only needs to be updated if it becomes a 2-2 node
      if (sibling_alter_child_has_rank_diff_2) {
        // Demote a 2-2 cursor if it is a leaf
        bool cursor_is_leaf = cursor->is_leaf();
        if (cursor_is_leaf)
          cursor->clear_flags();

        // If cursor is now a leaf, then its parent (which should be the pivot)
        // becomes a 2-2 node after cursor's demotion. Otherwise, cursor itself
        // should become a 2-2 node.
        WeakAVLNode *candidate = cursor_is_leaf ? new_subroot : cursor;
        candidate->toggle_rank_diff_2(sibling_is_right ^ cursor_is_leaf);
        LIBC_ASSERT(candidate->both_flags_set() &&
                    "target node should become a 2-2 node.");
      }
    }
    // Case 4. continue from Case 3; but rank-difference 1 child T of sibling
    // is on the opposite direction.
    //
    //             (P)                                     (P)
    //              | X                                     | X
    //             (C)               Zig-Zag               (T)
    //          1 /   \                =>                  / \
    //          (S)    \                                2 /   \ 2
    //         /   \ 1  \ 3                             (S)   (C)
    //    2   /     (T)  (D)                         1 / Y \ / Z \ 1
    //      (*)   Y /  \ Z                            (*) (A)(B) (D)
    //            (A)  (B)
    else {
      WeakAVLNode *target_child = rotate(root, sibling, !sibling_is_right);
      bool subtree_left_diff_2 = target_child->left_rank_diff_2;
      bool subtree_right_diff_2 = target_child->right_rank_diff_2;
      [[maybe_unused]] WeakAVLNode *new_subroot =
          rotate(root, cursor, sibling_is_right);
      LIBC_ASSERT(new_subroot == target_child &&
                  "target_child should become the subtree root");
      // Set flags
      target_child->set_both_flags();
      cursor->clear_flags();
      sibling->clear_flags();
      // Select destinations
      WeakAVLNode *dst_left = sibling_is_right ? cursor : sibling;
      WeakAVLNode *dst_right = sibling_is_right ? sibling : cursor;
      // Masked toggles
      if (subtree_left_diff_2)
        dst_left->toggle_rank_diff_2(true);
      if (subtree_right_diff_2)
        dst_right->toggle_rank_diff_2(false);
    }
  }

  enum struct WalkType {
    PreOrder,
    InOrder,
    PostOrder,
    Leaf,
  };
  template <typename Func>
  LIBC_INLINE static void walk(const WeakAVLNode *node, Func func,
                               int depth = 0) {
    if (!node)
      return;

    if (node->is_leaf()) {
      func(node, WalkType::Leaf, depth);
      return;
    }

    func(node, WalkType::PreOrder, depth);
    if (node->children[0])
      walk(node->children[0], func, depth + 1);

    func(node, WalkType::InOrder, depth);

    if (node->children[1])
      walk(node->children[1], func, depth + 1);
    func(node, WalkType::PostOrder, depth);
  }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_WEAK_AVL_H
