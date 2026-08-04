// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_HAMT_H_
#define V8_MAGLEV_HAMT_H_

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
 * HAMT merge_into(const HAMT& other, Func&& f) const
 * - Time:  O(N) (worst case)
 * - Space: O(N) (if copying is needed)
 * - Description: Returns a new tree where values in 'this' are updated
 * using values from 'other' via the functor 'f'. This operation only
 * affects keys present in 'this' (intersection update).
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
    return HAMT(MergeIntoRec(zone, root_, other.root_, 0, f));
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

  template <typename Func>
  Leaf* MergeCollisionLists(Zone* zone, Leaf* list1, Leaf* list2,
                            Func&& f) const {
    Leaf* new_list = nullptr;
    for (Leaf* cur = list1; cur; cur = cur->next) {
      Value* value = FindInCollisionList(list2, cur->key);
      new_list =
          NewLeaf(zone, cur->key, value ? f(cur->value, *value) : cur->value,
                  cur->hash, new_list);
    }
    return new_list;
  }

  template <typename Func>
  Branch* MergeIntoBranch(Zone* zone, Branch* branch, Leaf* leaf, int shift,
                          Func&& f) const {
    int bit = (leaf->hash >> shift) & kMask;
    if (!branch->has_child_in_bit(bit)) return branch;

    int idx = branch->get_index(bit);
    Node merged = MergeIntoRec(zone, branch->get_child(idx), Node(leaf),
                               shift + kBits, f);

    if (branch->get_child(idx) == merged) return branch;

    Branch* new_branch = CloneBranch(zone, branch);
    new_branch->set_child(idx, merged);
    return new_branch;
  }

  template <typename Func>
  Leaf* MergeIntoLeaf(Zone* zone, Leaf* list, Branch* branch, int shift,
                      Func&& f) const {
    Leaf* new_list = nullptr;
    for (Leaf* cur = list; cur; cur = cur->next) {
      const Value* value = FindHelper(Node(branch), cur->key, cur->hash, shift);
      new_list =
          NewLeaf(zone, cur->key, value ? f(cur->value, *value) : cur->value,
                  cur->hash, new_list);
    }
    return new_list;
  }

  template <typename Func>
  Node MergeIntoRec(Zone* zone, Node a, Node b, int shift, Func&& f) const {
    if (a == b) return a;  // Always return the left side!
    if (a.IsNull() || b.IsNull()) return a;

    if (a.type() == kLeaf && b.type() == kLeaf) {
      return Node(MergeCollisionLists(zone, a.AsLeaf(), b.AsLeaf(), f));
    }
    if (a.type() == kLeaf) {
      return Node(MergeIntoLeaf(zone, a.AsLeaf(), b.AsBranch(), shift, f));
    }
    if (b.type() == kLeaf) {
      return Node(MergeIntoBranch(zone, a.AsBranch(), b.AsLeaf(), shift, f));
    }

    // Both are Branches.
    Branch* brA = a.AsBranch();
    Branch* brB = b.AsBranch();
    uint32_t mapA = brA->bitmap;
    uint32_t mapB = brB->bitmap;
    Branch* res = CloneBranch(zone, brA);
    Node* res_children = res->children;
    int child_idx = 0;
    for (int i = 0; i < 32; ++i) {
      uint32_t bit = (1U << i);
      if (!(mapA & bit)) continue;
      child_idx++;
      if (!(mapB & bit)) continue;
      Node child =
          MergeIntoRec(zone, brA->get_child(brA->get_index(i)),
                       brB->get_child(brB->get_index(i)), shift + kBits, f);
      res_children[child_idx - 1] = child;
    }
    return Node(res);
  }
};

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_HAMT_H_
