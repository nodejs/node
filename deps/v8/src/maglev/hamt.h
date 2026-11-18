// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_HAMT_H_
#define V8_MAGLEV_HAMT_H_

#include <bit>

#include "src/base/pointer-with-payload.h"
#include "src/zone/zone.h"

namespace v8::internal::maglev {

class HAMTTest;

/**
 * A persistent (immutable) Hash Array Mapped Trie (HAMT).
 *
 * Characteristics:
 * - Persistent: Insertions return a new tree sharing structure with the
 *               old one.
 * - Zone-allocated: Nodes are allocated efficiently in a specific memory zone.
 * - Structure: Uses 5-bit chunks of the hash to index into 32-way Branch nodes.
 * - Compression: Branch nodes use a bitmap to store only present children
 *                (dense array).
 * - Collisions: Leaf nodes use a linked list for hash collisions.
 *               Note that the collision only happens if 60 bits are the same!
 *               (30 bits for 32-bit systems).
 *
 * API & Complexity:
 *
 * const Value* find(const Key& key) const
 * - Time:  O(log32 N)
 * - Space: O(1)
 * - Description: Returns a pointer to the value associated with the key,
 * or nullptr if the key is not present.
 *
 * HAMT insert(Key key, Value value) const
 * - Time:  O(log32 N)
 * - Space: O(log32 N) (Due to path copying)
 * - Description: Returns a new tree with the key inserted.
 * - Note on existing keys: If the key already exists, the value is updated
 * in the new tree. If the new value is identical to the existing value,
 * the operation is a no-op and the existing structure is returned.
 *
 * HAMT merge_into(Zone* zone, const HAMT& other, Func&& f) const
 * - Time:  O(N) (worst case; subtrees shared by pointer are skipped)
 * - Space: O(C * log32 N), where C is the number of values changed by 'f'
 * - Description: Returns a new tree where values in 'this' are updated
 * using values from 'other' via the functor 'f'. This operation only
 * affects keys present in 'this' (left join).
 * - Note on sharing: Like insert, merging is copy-on-write. Only nodes on
 * the path to a changed value are copied. If 'f' changes no value, the
 * operation is a no-op and the existing structure is returned.
 *
 * HAMT intersect(Zone* zone, const HAMT& other, Func&& f) const
 * - Description: Like merge_into, but drops keys absent from either tree.
 *
 * Data Structure Diagram:
 *
 * Hash (size_t)
 * +-----+- ... -------+-------+-------+
 * | X X |   ...       | Lvl 1 | Lvl 0 |  <-- 5 bits per level
 * +-----+- ... -------+-------+-------+
 * |   |
 * |   +-> Last 4 bits are unused. After that we consider a hash collision.
 * v
 * [ Branch (Lvl 0, Shift 0)]
 * +---------------------------+
 * | Bitmap: 0..010010 (32bit) |  <-- Indicates which indexes exist
 * +---------------------------+
 * | Children[] (Dense Array)  |
 * | [0]: Node ----------------------> [ Leaf ]
 * | [1]: Node --+             |       +-----------------------+
 * +--------------|------------+       | Key: "A", Value: 1    |
 *                |                    | Next: nullptr         |
 *                v                    +-----------------------+
 *               [ Branch (Lvl 1, Shift 5) ]
 *               +-------------------------+
 *               | Bitmap: ...             |
 *               +-------------------------+
 *               | ...                     |
 */
template <typename Key, typename Value>
class HAMT {
 public:
  using Hash = size_t;
  class Iterator;

  enum NodeType : uint8_t { kLeaf, kBranch };

  struct Branch;
  struct Leaf;

  struct Node {
    explicit Node() : payload_(nullptr, kLeaf) {}
    explicit Node(Leaf* leaf) : payload_(leaf, kLeaf) {}
    explicit Node(Branch* branch) : payload_(branch, kBranch) {}

    bool operator==(const Node& other) const {
      return payload_.raw() == other.payload_.raw();
    }

    bool IsNull() const {
      static_assert(kLeaf == 0);
      return payload_.raw() == 0;
    }

    NodeType type() const {
      DCHECK(!IsNull());
      return payload_.GetPayload();
    }

    Leaf* AsLeaf() const {
      DCHECK_EQ(type(), kLeaf);
      return reinterpret_cast<Leaf*>(
          payload_.GetPointerWithKnownPayload(NodeType::kLeaf));
    }

    Branch* AsBranch() const {
      DCHECK_EQ(type(), kBranch);
      return reinterpret_cast<Branch*>(
          payload_.GetPointerWithKnownPayload(NodeType::kBranch));
    }

   private:
    base::PointerWithPayload<void, NodeType, 1> payload_;
  };

  // Branch nodes contain a bitmap and array of nodes (allocated after the
  // struct). The bitmap indicates which of the kBits of the hashed key is in
  // the array of nodes. The array of nodes is dense (no holes).
  struct alignas(8) Branch {
    uint32_t bitmap;
    Node children[];

    explicit Branch(uint32_t bitmap) : bitmap(bitmap) {}

    // Number of nodes in this branch level.
    int size() const { return std::popcount(bitmap); }

    bool has_child_in_bit(int bit) const { return bitmap & (1U << bit); }

    int get_index(int bit) const {
      return std::popcount(bitmap & ((1U << bit) - 1));
    }

    const Node get_child(int idx) const { return children[idx]; }

    Node get_child(int idx) { return children[idx]; }

    void set_child(int idx, Node node) { children[idx] = node; }
  };

  // Leaf nodes contain a key/value pair.
  struct alignas(8) Leaf {
    Key key;
    Value value;
    Hash hash;
    Leaf* next;  // Chaining for hash collisions.

    Leaf(Key key, Value value, Hash hash, Leaf* next)
        : key(key), value(value), hash(hash), next(next) {}
  };

  HAMT() = default;
  HAMT(const HAMT& hamt) V8_NOEXCEPT = default;
  HAMT& operator=(const HAMT&) V8_NOEXCEPT = default;

  Iterator begin() const { return Iterator(root_); }
  Iterator end() const { return Iterator(Node()); }

  HAMT insert(Zone* zone, Key key, Value value) const {
    Hash hash = base::hash<Key>()(key);
    return HAMT(InsertRec(zone, root_, key, value, hash, 0));
  }

  const Value* find(const Key& key) const {
    Hash hash = base::hash<Key>()(key);
    return FindHelper(root_, key, hash, 0);
  }

  // WARNING: This is not symmetrical.
  // Merges 'other' into 'this'. If a key exists in both, 'f' is called to
  // resolve the conflict: f(this_value, other_value).
  template <typename Func>
    requires std::is_invocable_r_v<Value, Func, Value, Value>
  HAMT merge_into(Zone* zone, const HAMT& other, Func&& f) const {
    if (root_ == other.root_) return *this;
    if (root_.IsNull()) return *this;
    if (other.root_.IsNull()) return *this;
    return HAMT(
        MergeIntoRec<MergeMode::kLeftJoin>(zone, root_, other.root_, 0, f));
  }

  // Intersects 'this' and 'other'. Only keys present in both trees are kept,
  // with their values combined via f(this_value, other_value). Shared subtrees
  // are reused without calling 'f', so f(v, v) must be v.
  template <typename Func>
    requires std::is_invocable_r_v<Value, Func, Value, Value>
  HAMT intersect(Zone* zone, const HAMT& other, Func&& f) const {
    if (root_ == other.root_) return *this;
    if (root_.IsNull() || other.root_.IsNull()) return HAMT();
    return HAMT(
        MergeIntoRec<MergeMode::kIntersect>(zone, root_, other.root_, 0, f));
  }

  /**
   * Iterator for traversing all entries in the HAMT.
   *
   * Iteration Strategy:
   * This iterator implements a non-recursive Depth-First Search (DFS)
   * traversal.
   * - It maintains a `stack_` of `Branch` nodes representing the path from the
   * root down to the current level.
   * - Each stack entry tracks the current index being visited within that
   * branch's dense child array.
   * - Hash Collisions: When a Leaf is encountered, `current_` iterates through
   * the linked list of collisions (if any) before the iterator backtracks
   * to the parent branch to find the next sibling.
   *
   * Ordering Note:
   * Iteration order is determined by the hash layout (implementation detail),
   * NOT by insertion order or key sorting. It should be considered
   * unpredictable.
   */
  class Iterator {
   public:
    struct StackEntry {
      Branch* node;
      int idx;  // Current child index in the dense array.
    };

    explicit Iterator(Node root) : current_(nullptr) { Push(root); }

    bool operator==(const Iterator& other) const {
      return current_ == other.current_;
    }
    bool operator!=(const Iterator& other) const { return !(*this == other); }

    void operator++() {
      if (!current_) return;

      // Move to next collision in chain.
      if (current_->next) {
        current_ = current_->next;
        return;
      }

      // Backtrack up the tree.
      while (!stack_.empty()) {
        StackEntry& entry = stack_.back();
        entry.idx++;
        if (entry.idx < entry.node->size()) {
          // Found a sibling, descend.
          Push(entry.node->get_child(entry.idx));
          return;
        }
        stack_.pop_back();
      }

      // Done.
      current_ = nullptr;
    }

    const std::pair<Key, Value> operator*() const {
      return {current_->key, current_->value};
    }

   private:
    const Leaf* current_;
    std::vector<StackEntry> stack_;

    void Push(Node node) {
      while (!node.IsNull()) {
        if (node.type() == kLeaf) {
          current_ = node.AsLeaf();
          return;
        }
        DCHECK_EQ(node.type(), kBranch);
        Branch* branch = node.AsBranch();
        stack_.push_back({branch, 0});
        node = branch->get_child(0);
      }
    }
  };

 private:
  Node root_;

  friend HAMTTest;

  // Each level uses 5 bits from the hash.
  static constexpr int kBits = 5;
  static constexpr int kWidth = 1 << kBits;
  static constexpr int kMask = kWidth - 1;
  // Max shift done by the last level (max tree height).
  static constexpr int kMaxShift = (sizeof(Hash) * 8) - kBits;

  explicit HAMT(Node root) : root_(root) {}

  Leaf* NewLeaf(Zone* zone, Key key, Value value, size_t hash,
                Leaf* next = nullptr) const {
    return zone->New<Leaf>(key, value, hash, next);
  }

  Branch* NewBranch(Zone* zone, uint32_t bitmap, int count) const {
    DCHECK_EQ(count, std::popcount(bitmap));
    size_t total_bytes = sizeof(Branch) + (count * sizeof(Node));
    void* branch = zone->Allocate<void*>(total_bytes);
    return new (branch) Branch(bitmap);
  }

  Branch* CloneBranch(Zone* zone, Branch* old_branch) const {
    Branch* new_branch =
        NewBranch(zone, old_branch->bitmap, old_branch->size());
    std::memcpy(new_branch->children, old_branch->children,
                old_branch->size() * sizeof(Node));
    return new_branch;
  }

  Leaf* InsertInCollisionListRec(Zone* zone, Leaf* leaf, const Key& key,
                                 const Value& value, Hash hash) const {
    if (!leaf) {
      return NewLeaf(zone, key, value, hash);
    }

    if (leaf->key == key) {
      if (leaf->value == value) return leaf;
      return NewLeaf(zone, key, value, hash, leaf->next);
    }

    Leaf* new_next =
        InsertInCollisionListRec(zone, leaf->next, key, value, hash);
    if (new_next == leaf->next) return leaf;

    return NewLeaf(zone, leaf->key, leaf->value, leaf->hash, new_next);
  }

  Node InsertInLeaf(Zone* zone, Leaf* leaf, const Key& key, const Value& value,
                    Hash hash, int shift) const {
    // Same key.
    if (leaf->key == key) {
      if (leaf->value == value) return Node(leaf);
      return Node(NewLeaf(zone, key, value, hash, leaf->next));
    }

    // Hash collision or max depth.
    if (leaf->hash == hash || shift > kMaxShift) {
      return Node(InsertInCollisionListRec(zone, leaf, key, value, hash));
    }

    // Partial hash match. Add a new tree level: we need to push the existing
    // leaf deeper and insert the new key.
    int old_bit = (leaf->hash >> shift) & kMask;
    int new_bit = (hash >> shift) & kMask;

    // Bits are different, so we will stop in the next level.
    // Just create the branch directly.
    if (old_bit != new_bit) {
      Leaf* new_leaf = NewLeaf(zone, key, value, hash);
      uint32_t bitmap = (1U << old_bit) | (1U << new_bit);
      Branch* branch = NewBranch(zone, bitmap, 2);
      branch->set_child(0, Node(old_bit < new_bit ? leaf : new_leaf));
      branch->set_child(1, Node(old_bit < new_bit ? new_leaf : leaf));
      return Node(branch);
    }

    // Bitmaps are the same. Recurse.
    Node child = InsertRec(zone, Node(leaf), key, value, hash, shift + kBits);
    Branch* branch = NewBranch(zone, 1U << old_bit, 1);
    branch->set_child(0, child);
    return Node(branch);
  }

  Node InsertRec(Zone* zone, Node node, const Key& key, const Value& value,
                 Hash hash, int shift) const {
    if (node.IsNull()) {
      return Node(NewLeaf(zone, key, value, hash));
    }

    if (node.type() == kLeaf) {
      return InsertInLeaf(zone, node.AsLeaf(), key, value, hash, shift);
    }

    // Branch case.
    DCHECK_EQ(node.type(), kBranch);
    Branch* branch = node.AsBranch();
    int bit = (hash >> shift) & kMask;
    int idx = branch->get_index(bit);

    if (branch->has_child_in_bit(bit)) {
      // Child already exits at this bit.
      Node existing_child = branch->get_child(idx);
      Node new_child =
          InsertRec(zone, existing_child, key, value, hash, shift + kBits);

      // No change.
      if (existing_child == new_child) return Node(branch);

      Branch* copy = CloneBranch(zone, branch);
      copy->set_child(idx, new_child);
      return Node(copy);
    }

    // New bit. Add a new leaf and insert into a new copy of a branch node.
    Node new_child = Node(NewLeaf(zone, key, value, hash));

    uint32_t new_map = branch->bitmap | (1U << bit);
    int old_count = branch->size();
    Branch* copy = NewBranch(zone, new_map, old_count + 1);
    DCHECK_EQ(std::popcount(new_map), old_count + 1);
    Node* src = branch->children;
    Node* dst = copy->children;

    // Memcpy split around idx.
    if (idx > 0) std::memcpy(dst, src, idx * sizeof(Node));
    dst[idx] = new_child;
    if (idx < old_count) {
      std::memcpy(dst + idx + 1, src + idx, (old_count - idx) * sizeof(Node));
    }
    return Node(copy);
  }

  const Value* FindHelper(Node node, const Key& key, Hash hash,
                          int shift) const {
    while (!node.IsNull()) {
      if (node.type() == kLeaf) {
        return FindInCollisionList(node.AsLeaf(), key);
      }
      DCHECK_EQ(node.type(), kBranch);
      const Branch* branch = node.AsBranch();
      int bit = (hash >> shift) & kMask;
      if (!branch->has_child_in_bit(bit)) return nullptr;
      int idx = branch->get_index(bit);
      node = branch->get_child(idx);
      shift += kBits;
    }
    return nullptr;
  }

  Value* FindInCollisionList(Leaf* list, Key key) const {
    for (Leaf* cur = list; cur; cur = cur->next) {
      if (cur->key == key) {
        return &cur->value;
      }
    }
    return nullptr;
  }

  enum class MergeMode { kLeftJoin, kIntersect };

  static bool HasSingleHash(const Leaf* leaf) {
    for (const Leaf* c = leaf->next; c != nullptr; c = c->next) {
      if (c->hash != leaf->hash) return false;
    }
    return true;
  }

  // Merges list2 into list1, i.e. keys that are only in list2 are dropped.
  // A leaf is only reallocated if its value, or the value of a leaf behind it
  // in the chain, actually changes.
  template <MergeMode kMode, typename Func>
  Leaf* MergeCollisionLists(Zone* zone, Leaf* list1, Leaf* list2,
                            Func&& f) const {
    if (list1 == nullptr) return nullptr;
    Leaf* new_next = MergeCollisionLists<kMode>(zone, list1->next, list2, f);
    Value* value = FindInCollisionList(list2, list1->key);
    if (kMode == MergeMode::kIntersect && !value) return new_next;
    Value merged = value ? f(list1->value, *value) : list1->value;
    if (new_next == list1->next && merged == list1->value) {
      return list1;
    }
    return NewLeaf(zone, list1->key, merged, list1->hash, new_next);
  }

  template <MergeMode kMode, typename Func>
  Node MergeIntoBranch(Zone* zone, Branch* branch, Leaf* leaf, int shift,
                       Func&& f) const {
    int bit = (leaf->hash >> shift) & kMask;
    if (!branch->has_child_in_bit(bit)) {
      return kMode == MergeMode::kLeftJoin ? Node(branch) : Node();
    }

    int idx = branch->get_index(bit);
    Node merged = MergeIntoRec<kMode>(zone, branch->get_child(idx), Node(leaf),
                                      shift + kBits, f);
    if constexpr (kMode == MergeMode::kIntersect) return merged;

    if (branch->get_child(idx) == merged) return Node(branch);

    Branch* new_branch = CloneBranch(zone, branch);
    new_branch->set_child(idx, merged);
    return Node(new_branch);
  }

  // Merges the entries of branch into the collision chain list. Leaves are
  // reused unless their value changes.
  template <MergeMode kMode, typename Func>
  Leaf* MergeIntoLeaf(Zone* zone, Leaf* list, Branch* branch, int shift,
                      Func&& f) const {
    if (list == nullptr) return nullptr;
    Leaf* new_next = MergeIntoLeaf<kMode>(zone, list->next, branch, shift, f);
    const Value* value = FindHelper(Node(branch), list->key, list->hash, shift);
    if (kMode == MergeMode::kIntersect && !value) return new_next;
    Value merged = value ? f(list->value, *value) : list->value;
    if (new_next == list->next && merged == list->value) {
      return list;
    }
    return NewLeaf(zone, list->key, merged, list->hash, new_next);
  }

  template <MergeMode kMode, typename Func>
  Node MergeIntoRec(Zone* zone, Node a, Node b, int shift, Func&& f) const {
    if (a == b) return a;  // Always return the left side!
    if (a.IsNull() || b.IsNull())
      return kMode == MergeMode::kLeftJoin ? a : Node();

    if (a.type() == kLeaf && b.type() == kLeaf) {
      return Node(MergeCollisionLists<kMode>(zone, a.AsLeaf(), b.AsLeaf(), f));
    }
    if (a.type() == kLeaf) {
      return Node(
          MergeIntoLeaf<kMode>(zone, a.AsLeaf(), b.AsBranch(), shift, f));
    }
    if (b.type() == kLeaf) {
      return MergeIntoBranch<kMode>(zone, a.AsBranch(), b.AsLeaf(), shift, f);
    }

    // Both are Branches.
    Branch* brA = a.AsBranch();
    Branch* brB = b.AsBranch();
    uint32_t shared_bits = brA->bitmap & brB->bitmap;
    if constexpr (kMode == MergeMode::kLeftJoin) {
      Branch* res = nullptr;
      for (uint32_t bits = shared_bits; bits != 0; bits &= bits - 1) {
        int bit = std::countr_zero(bits);
        int idx = brA->get_index(bit);
        Node old_child = brA->get_child(idx);
        Node child = MergeIntoRec<kMode>(zone, old_child,
                                         brB->get_child(brB->get_index(bit)),
                                         shift + kBits, f);
        if (child == old_child) continue;
        if (res == nullptr) res = CloneBranch(zone, brA);
        res->set_child(idx, child);
      }
      return res == nullptr ? a : Node(res);
    } else {
      Node new_children[kWidth];
      uint32_t new_bitmap = 0;
      int new_count = 0;
      bool same_as_a = (shared_bits == brA->bitmap);
      bool same_as_b = (shared_bits == brB->bitmap);
      for (uint32_t bits = shared_bits; bits != 0; bits &= bits - 1) {
        int bit = std::countr_zero(bits);
        Node child_a = brA->get_child(brA->get_index(bit));
        Node child_b = brB->get_child(brB->get_index(bit));
        Node child =
            MergeIntoRec<kMode>(zone, child_a, child_b, shift + kBits, f);
        same_as_a &= (child == child_a);
        same_as_b &= (child == child_b);
        if (!child.IsNull()) {
          new_bitmap |= (1U << bit);
          new_children[new_count++] = child;
        }
      }
      if (new_count == 0) return Node();
      if (same_as_a) return a;
      if (same_as_b) return b;
      if (new_count == 1 && new_children[0].type() == kLeaf &&
          HasSingleHash(new_children[0].AsLeaf())) {
        return new_children[0];
      }
      Branch* res = NewBranch(zone, new_bitmap, new_count);
      std::memcpy(res->children, new_children, new_count * sizeof(Node));
      return Node(res);
    }
  }
};

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_HAMT_H_
