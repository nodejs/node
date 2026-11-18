// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_KNA_MAP_H_
#define V8_MAGLEV_MAGLEV_KNA_MAP_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "src/base/compiler-specific.h"
#include "src/base/logging.h"
#include "src/base/sanitizer/asan.h"
#include "src/base/small-vector.h"
#include "src/flags/flags.h"
#include "src/maglev/maglev-ir.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace maglev {

struct DefaultMergeFunc {
  template <typename Key, typename Value>
  bool operator()(const Key&, const Value& lhs, const Value& rhs) const {
    return lhs == rhs;
  }
};

struct NeverKeepLhsOnly {
  template <typename Key, typename Value>
  bool operator()(const Key&, Value&) const {
    return false;
  }
};

#ifdef V8_ENABLE_MAGLEV_KNA_BTREE

// A Copy-on-Write (CoW) B+Tree map backed by a Zone-allocated freelist pool.
//
// Values live exclusively in LeafNodes (up to kMaxKeys sorted key-value pairs
// per leaf); InternalNodes hold only separator keys and child pointers. Cloning
// a map is O(1) (increments root_->ref_count). Mutations unshare only the
// root-to-leaf path on demand, and nodes whose ref_count drops to 0 are
// recycled via Pool's intrusive freelists.
//
// Pointer / reference invalidation contract:
// - Raw pointers and references to entries (`Value&` from `operator[]`, or
//   `iterator` from `find()`/`try_emplace()`/`emplace()`/`insert()`) are
//   invalidated by any subsequent insertion, erasure, merge, or copy
//   construction/assignment (`Clone`) of the map, which may shift entries
//   within a leaf, split/coalesce leaves, or unshare a leaf into a new node.
template <typename Key, typename Value, size_t kMaxKeys = 8>
class KnaMap {
  static_assert(kMaxKeys >= 4 && kMaxKeys % 2 == 0);
  static_assert(std::is_trivially_destructible_v<Key>);
  static constexpr size_t kMinKeys = kMaxKeys / 2;
  static constexpr size_t kMaxChildren = kMaxKeys + 1;
  // With kMinKeys >= 2 (minimum internal branching factor >= 3, or 5 for
  // kMaxKeys = 8), depth 12 supports > 4 * 5^12 (> 9e8) entries, far exceeding
  // any Maglev compilation unit.
  static constexpr size_t kMaxDepth = 12;

  template <typename K, typename V, size_t M>
  friend class KnaMap;
  friend class KnaMapTest;

  struct TreeNodeBase;
  struct LeafNode;
  struct InternalNode;

 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<Key, Value>;

  struct Pool {
    Zone* zone;
    LeafNode* free_leaves = nullptr;
    InternalNode* free_internals = nullptr;
    explicit Pool(Zone* z) : zone(z) {}
  };

  class const_iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = KnaMap::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    const_iterator() = default;
    const_iterator(const LeafNode* leaf, uint16_t idx)
        : leaf_(leaf), idx_(idx) {
#ifdef DEBUG
      can_advance_ = false;
#endif
    }

    explicit const_iterator(const TreeNodeBase* root) {
      if (!root) return;
      DescendToLeftmost(root);
    }

    reference operator*() const { return leaf_->entries()[idx_]; }
    pointer operator->() const { return &leaf_->entries()[idx_]; }

    const_iterator& operator++() {
      DCHECK_NOT_NULL(leaf_);
      DCHECK(can_advance_);
      if (++idx_ < leaf_->num_keys) {
        return *this;
      }
      AdvanceToNextLeaf();
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const const_iterator& other) const {
      return leaf_ == other.leaf_ && idx_ == other.idx_;
    }
    bool operator!=(const const_iterator& other) const {
      return !(*this == other);
    }

   private:
    friend class KnaMap;
    friend class direct_iterator;
    friend class lazy_find_iterator;

    void SkipCurrentLeaf() {
      DCHECK_NOT_NULL(leaf_);
      DCHECK(can_advance_);
      AdvanceToNextLeaf();
    }

    const LeafNode* CurrentLeaf() const { return leaf_; }
    uint16_t CurrentIndex() const { return idx_; }

    struct StackEntry {
      const InternalNode* node;
      uint16_t child_idx;
    };

    void DescendToLeftmost(const TreeNodeBase* curr) {
      while (!curr->is_leaf) {
        const InternalNode* internal = static_cast<const InternalNode*>(curr);
        DCHECK_LT(depth_, kMaxDepth);
        stack_[depth_++] = {internal, 0};
        curr = internal->children[0];
      }
      leaf_ = static_cast<const LeafNode*>(curr);
      idx_ = 0;
    }

    void AdvanceToNextLeaf() {
      while (depth_ > 0) {
        StackEntry& top = stack_[depth_ - 1];
        if (++top.child_idx <= top.node->num_keys) {
          DescendToLeftmost(top.node->children[top.child_idx]);
          return;
        }
        --depth_;
      }
      leaf_ = nullptr;
      idx_ = 0;
    }

    const LeafNode* leaf_ = nullptr;
    uint16_t idx_ = 0;
    uint16_t depth_ = 0;
    StackEntry stack_[kMaxDepth];
#ifdef DEBUG
    bool can_advance_ = true;
#endif
  };

  class iterator {
   public:
    explicit iterator(value_type* ptr = nullptr) : ptr_(ptr) {}
    value_type& operator*() const { return *ptr_; }
    value_type* operator->() const { return ptr_; }
    bool operator==(const iterator& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const iterator& other) const { return ptr_ != other.ptr_; }
    bool operator==(const const_iterator& other) const {
      return ptr_ == (other.CurrentLeaf() ? &(*other) : nullptr);
    }
    bool operator!=(const const_iterator& other) const {
      return !(*this == other);
    }

   private:
    value_type* ptr_ = nullptr;
  };

  explicit KnaMap(Pool* pool) V8_NOEXCEPT : pool_(pool) {
    DCHECK_NOT_NULL(pool_);
  }

  KnaMap(const KnaMap& other) V8_NOEXCEPT : root_(other.root_),
                                            pool_(other.pool_),
                                            size_(other.size_) {
    AddRef(root_);
  }

  template <typename Func>
  KnaMap(const KnaMap& other, Func&& mutate) : KnaMap(other) {
    MutateValues(std::forward<Func>(mutate));
  }

  KnaMap(KnaMap&& other) V8_NOEXCEPT : root_(other.root_),
                                       pool_(other.pool_),
                                       size_(other.size_) {
    other.root_ = nullptr;
    other.size_ = 0;
  }

  ~KnaMap() { Release(root_); }

  KnaMap& operator=(const KnaMap& other) V8_NOEXCEPT {
    if (this != &other) {
      DCHECK_EQ(pool_->zone, other.pool_->zone);
      AddRef(other.root_);
      Release(root_);
      root_ = other.root_;
      pool_ = other.pool_;
      size_ = other.size_;
    }
    return *this;
  }

  KnaMap& operator=(KnaMap&& other) V8_NOEXCEPT {
    if (this != &other) {
      DCHECK_EQ(pool_->zone, other.pool_->zone);
      Release(root_);
      root_ = other.root_;
      pool_ = other.pool_;
      size_ = other.size_;
      other.root_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  bool empty() const { return size_ == 0; }
  size_t size() const { return size_; }

  void clear() {
    Release(root_);
    root_ = nullptr;
    size_ = 0;
  }

  const_iterator begin() const { return const_iterator(root_); }
  const_iterator end() const { return const_iterator(); }
  const_iterator cbegin() const { return const_iterator(root_); }
  const_iterator cend() const { return const_iterator(); }

  const_iterator find(const Key& k) const { return FindConstIterator(k); }

  iterator find(const Key& k) { return iterator(FindMutableEntry(k)); }

  bool contains(const Key& k) const { return FindEntry(k) != nullptr; }

  Value& operator[](const Key& k) { return InsertImpl(k).first->second; }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
    auto [ptr, inserted] = InsertImpl(k, std::forward<Args>(args)...);
    return {iterator(ptr), inserted};
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(const Key& k, Args&&... args) {
    auto [ptr, inserted] = InsertImpl(k, std::forward<Args>(args)...);
    return {iterator(ptr), inserted};
  }

  std::pair<iterator, bool> emplace(const value_type& entry) {
    auto [ptr, inserted] = InsertImpl(entry.first, entry.second);
    return {iterator(ptr), inserted};
  }

  std::pair<iterator, bool> insert(const value_type& entry) {
    auto [ptr, inserted] = InsertImpl(entry.first, entry.second);
    return {iterator(ptr), inserted};
  }

  size_t erase(const Key& k) {
    if (!root_) return 0;
    PathEntry path[kMaxDepth];
    size_t depth = 0;
    TreeNodeBase* curr = root_;
    while (!curr->is_leaf) {
      InternalNode* internal = static_cast<InternalNode*>(curr);
      size_t idx = FindChildIndex(internal, k);
      DCHECK_LT(depth, kMaxDepth);
      path[depth++] = {internal, static_cast<uint16_t>(idx)};
      curr = internal->children[idx];
    }
    LeafNode* leaf = static_cast<LeafNode*>(curr);
    size_t pos = LowerBound(leaf, k);
    if (pos == leaf->num_keys || !(leaf->key(pos) == k)) return 0;
    if (depth == 0 || leaf->num_keys > kMinKeys) {
      if (depth == 0 && leaf->num_keys == 1) {
        Release(root_);
        root_ = nullptr;
        size_ = 0;
        return 1;
      }
      leaf = UnsharePathToLeaf(path, depth, leaf);
      value_type* entries = leaf->entries();
      size_t count = leaf->num_keys;
      for (size_t i = pos; i + 1 < count; ++i) {
        entries[i] = std::move(entries[i + 1]);
      }
      entries[count - 1].~value_type();
      leaf->num_keys = static_cast<uint16_t>(count - 1);
      --size_;
      return 1;
    }
    return EraseIf([&](const Key& key, const Value&) { return key == k; });
  }

  // Visits all entries in ascending key order, allowing `fn(const Key&,
  // Value&)` to mutate `Value&` in place without changing keys or erasing
  // entries. Unshares only leaves where `fn` actually modifies `val`.
  template <typename MutateFn>
  void MutateValues(MutateFn&& fn) {
    if (!root_) return;
    PathEntry path[kMaxDepth];
    MutateValuesRec(root_, path, 0, /*path_unshared=*/true, fn);
  }

  // Rebuilds the map by applying `remap_key(const Key&)` to each key. If
  // multiple keys map to the same normalized key, `combine(Value& dst, const
  // Value& src)` merges subsequent colliding values into the first inserted
  // value (visited in ascending original-key order).
  template <typename RemapKeyFn, typename CombineFn>
  void RemapKeys(RemapKeyFn&& remap_key, CombineFn&& combine) {
    if (!root_) return;
    bool any_changed = false;
    for (const auto& [k, v] : *this) {
      if (!(remap_key(k) == k)) {
        any_changed = true;
        break;
      }
    }
    if (!any_changed) return;
    KnaMap remapped(pool_);
    for (const auto& [k, v] : *this) {
      auto [it, inserted] = remapped.try_emplace(remap_key(k), v);
      if (!inserted) {
        combine(it->second, v);
      }
    }
    *this = std::move(remapped);
  }

  template <typename RemapKeyFn>
  void RemapKeys(RemapKeyFn&& remap_key) {
    RemapKeys(std::forward<RemapKeyFn>(remap_key),
              [](Value& dst, const Value& src) { dst = src; });
  }

  // Visits entries in ascending key order, erasing those for which
  // `pred(const Key&, Value&)` returns true. `pred` may mutate `Value&` in
  // place for surviving entries. Preserves shared leaves when no entry in a
  // leaf is erased or modified.
  template <typename Pred>
  size_t EraseIf(Pred&& pred) {
    if (!root_) return 0;

    auto process_leaf = [&](LeafNode*& leaf, bool unshared,
                            auto&& unshare_fn) -> size_t {
      size_t old_count = leaf->num_keys;
      size_t write_idx = 0;
      for (size_t i = 0; i < old_count; ++i) {
        if (!unshared) {
          if constexpr (std::is_invocable_v<Pred, const Key&, const Value&>) {
            if (pred(leaf->key(i), std::as_const(leaf->value(i)))) {
              continue;
            }
            if (write_idx == i) {
              ++write_idx;
              continue;
            }
            unshare_fn(leaf);
            unshared = true;
            leaf->entries()[write_idx++] = std::move(leaf->entries()[i]);
            continue;
          } else {
            Value temp_val = leaf->value(i);
            if (pred(leaf->key(i), temp_val)) {
              continue;
            }
            if (write_idx == i && ValueEquals(temp_val, leaf->value(i))) {
              ++write_idx;
              continue;
            }
            unshare_fn(leaf);
            unshared = true;
            if (write_idx != i) {
              leaf->entries()[write_idx].first = leaf->key(i);
            }
            leaf->value(write_idx++) = std::move(temp_val);
            continue;
          }
        }
        if (!pred(leaf->key(i), leaf->value(i))) {
          if (write_idx != i) {
            leaf->entries()[write_idx] = std::move(leaf->entries()[i]);
          }
          ++write_idx;
        }
      }
      if (!unshared && write_idx > 0 && write_idx < old_count) {
        unshare_fn(leaf);
      }
      return write_idx;
    };

    if (root_->is_leaf) {
      LeafNode* leaf = static_cast<LeafNode*>(root_);
      size_t old_count = leaf->num_keys;
      size_t write_idx =
          process_leaf(leaf, leaf->ref_count == 1, [&](LeafNode*& l) {
            l = UnshareLeaf(l);
            root_ = l;
          });
      size_t erased = old_count - write_idx;
      if (erased == 0) return 0;
      if (write_idx == 0) {
        Release(root_);
        root_ = nullptr;
        size_ = 0;
        return erased;
      }
      for (size_t i = write_idx; i < old_count; ++i) {
        leaf->entries()[i].~value_type();
      }
      leaf->num_keys = static_cast<uint16_t>(write_idx);
      size_ -= static_cast<uint32_t>(erased);
      return erased;
    }

    base::SmallVector<LeafNode*, 64> old_leaves;
    CollectLeaves(root_, old_leaves);
    base::SmallVector<LeafNode*, 64> out_leaves;
    bool tree_detached = false;
    size_t leaf_idx = 0;
    // Transfers ownership of all leaves from the internal tree to `old_leaves`
    // on the first mutation/erasure: increments each leaf's ref_count before
    // releasing `root_` (which drops the internal nodes' references), so leaves
    // exclusively owned by `*this` end up with `ref_count == 1`.
    auto ensure_detached = [&]() {
      if (tree_detached) return;
      tree_detached = true;
      for (LeafNode* l : old_leaves) AddRef(l);
      Release(root_);
      root_ = nullptr;
      out_leaves.reserve(old_leaves.size());
      for (size_t j = 0; j < leaf_idx; ++j) {
        out_leaves.push_back(old_leaves[j]);
      }
    };

    size_t old_total_size = size_;
    size_t new_total_size = 0;
    for (; leaf_idx < old_leaves.size(); ++leaf_idx) {
      LeafNode* leaf = old_leaves[leaf_idx];
      size_t old_count = leaf->num_keys;
      size_t write_idx = process_leaf(
          leaf, tree_detached && (leaf->ref_count == 1), [&](LeafNode*& l) {
            ensure_detached();
            if (l->ref_count > 1) l = UnshareLeaf(l);
          });
      if (write_idx == 0) {
        ensure_detached();
        Release(leaf);
        continue;
      }
      if (write_idx < old_count) {
        for (size_t i = write_idx; i < old_count; ++i) {
          leaf->entries()[i].~value_type();
        }
        leaf->num_keys = static_cast<uint16_t>(write_idx);
      }
      new_total_size += write_idx;
      if (tree_detached) {
        AppendOrCoalesceLeaf(out_leaves, leaf);
      }
    }

    if (!tree_detached) return 0;
    BuildTreeFromLeaves(out_leaves, new_total_size);
    return old_total_size - new_total_size;
  }

  // Destructively merges `rhs` into `*this` in ascending key order (`rhs` must
  // be a distinct `KnaMap` instance):
  // - Keys in both maps: `merge(const Key&, Value& lhs, const Value& rhs)` may
  //   mutate `lhs` and returns true to keep the entry, or false to erase it.
  //   For `Value == ValueNode*`, if `merge` returns false and either value is
  //   an Identity wrapper, `lhs` is updated to `lhs->UnwrapIdentities()` and
  //   `merge` is retried with the unwrapped values. When `KeepLhsOnlyFunc` is
  //   `NeverKeepLhsOnly`, `CanSkipSharedRange` skips shared roots and leaves
  //   once value-type-specific survival and side-effect invariants are met.
  // - Keys only in `*this`: `keep_lhs_only(const Key&, Value& lhs)` may mutate
  //   `lhs` and returns true to keep the entry, or false to erase it.
  // - Keys only in `rhs`: dropped.
  template <typename MergeFunc, typename KeepLhsOnlyFunc>
  void DestructivelyMergeWith(const KnaMap& rhs, MergeFunc&& merge,
                              KeepLhsOnlyFunc&& keep_lhs_only) {
    DCHECK_NE(this, &rhs);
    DCHECK_EQ(pool_->zone, rhs.pool_->zone);
    constexpr bool kNeverKeepLhsOnly =
        std::is_same_v<std::decay_t<KeepLhsOnlyFunc>, NeverKeepLhsOnly>;
    constexpr bool kIsDefaultMerge =
        std::is_same_v<std::decay_t<MergeFunc>, DefaultMergeFunc>;
    if (!root_) return;
    if (!rhs.root_) {
      if constexpr (kNeverKeepLhsOnly) {
        clear();
      } else {
        EraseIf([&](const Key& k, Value& v) { return !keep_lhs_only(k, v); });
      }
      return;
    }

    if (IsSharedWith(rhs) &&
        CanSkipSharedRange<kNeverKeepLhsOnly, kIsDefaultMerge>(begin(), end(),
                                                               merge)) {
      return;
    }

    const_iterator rhs_it = rhs.begin();
    const_iterator rhs_end = rhs.end();

    auto eval_entry = [&](const Key& k, Value& lhs_val,
                          const Value& rhs_val) -> bool {
      bool keep_value = merge(k, lhs_val, rhs_val);
      if constexpr (std::is_same_v<Value, ValueNode*>) {
        if (!keep_value && lhs_val && rhs_val) {
          auto l = lhs_val->UnwrapIdentities();
          auto r = rhs_val->UnwrapIdentities();
          if (l != lhs_val || r != rhs_val) {
            lhs_val = l;
            keep_value = merge(k, lhs_val, r);
          }
        }
      }
      return keep_value;
    };

    auto process_leaf = [&](LeafNode*& leaf, bool unshared,
                            auto&& unshare_fn) -> size_t {
      size_t old_count = leaf->num_keys;
      size_t write_idx = 0;
      for (size_t i = 0; i < old_count; ++i) {
        const Key& k = leaf->key(i);
        while (rhs_it != rhs_end && rhs_it->first < k) {
          ++rhs_it;
        }
        bool has_rhs = (rhs_it != rhs_end && rhs_it->first == k);
        const Value* rhs_val_ptr = has_rhs ? &rhs_it->second : nullptr;
        if (has_rhs) ++rhs_it;

        if constexpr (kNeverKeepLhsOnly) {
          if (!has_rhs) continue;
        }

        if (!unshared) {
          if constexpr (kNeverKeepLhsOnly && requires {
                          leaf->value(i).IsSharedWith(*rhs_val_ptr);
                        }) {
            // `ClearLoadedPropertiesForKey` clears an inner map in place
            // without erasing the outer entry, while `merge_loaded_properties`
            // returns `!lhs.empty()` to prune empty inner maps during merge.
            if (has_rhs && leaf->value(i).IsSharedWith(*rhs_val_ptr) &&
                !leaf->value(i).empty()) {
              if (write_idx != i) {
                unshare_fn(leaf);
                unshared = true;
                leaf->entries()[write_idx] = std::move(leaf->entries()[i]);
              }
              ++write_idx;
              continue;
            }
          }
          Value temp_val = leaf->value(i);
          bool keep = has_rhs ? eval_entry(k, temp_val, *rhs_val_ptr)
                              : keep_lhs_only(k, temp_val);
          if (!keep) continue;
          if (write_idx == i && ValueEquals(temp_val, leaf->value(i))) {
            ++write_idx;
            continue;
          }
          unshare_fn(leaf);
          unshared = true;
          if (write_idx != i) {
            leaf->entries()[write_idx].first = leaf->key(i);
          }
          leaf->value(write_idx++) = std::move(temp_val);
          continue;
        }

        bool keep = has_rhs ? eval_entry(k, leaf->value(i), *rhs_val_ptr)
                            : keep_lhs_only(k, leaf->value(i));
        if (keep) {
          if (write_idx != i) {
            leaf->entries()[write_idx] = std::move(leaf->entries()[i]);
          }
          ++write_idx;
        }
      }
      if (!unshared && write_idx > 0 && write_idx < old_count) {
        unshare_fn(leaf);
      }
      return write_idx;
    };

    if (root_->is_leaf) {
      LeafNode* leaf = static_cast<LeafNode*>(root_);
      size_t old_count = leaf->num_keys;
      size_t write_idx =
          process_leaf(leaf, leaf->ref_count == 1, [&](LeafNode*& l) {
            l = UnshareLeaf(l);
            root_ = l;
          });
      if (write_idx == old_count) return;
      if (write_idx == 0) {
        Release(root_);
        root_ = nullptr;
        size_ = 0;
        return;
      }
      for (size_t i = write_idx; i < old_count; ++i) {
        leaf->entries()[i].~value_type();
      }
      leaf->num_keys = static_cast<uint16_t>(write_idx);
      size_ = static_cast<uint32_t>(write_idx);
      return;
    }

    base::SmallVector<LeafNode*, 64> old_leaves;
    CollectLeaves(root_, old_leaves);
    base::SmallVector<LeafNode*, 64> out_leaves;
    bool tree_detached = false;
    size_t leaf_idx = 0;
    auto ensure_detached = [&]() {
      if (tree_detached) return;
      tree_detached = true;
      for (LeafNode* l : old_leaves) AddRef(l);
      Release(root_);
      root_ = nullptr;
      out_leaves.reserve(old_leaves.size());
      for (size_t j = 0; j < leaf_idx; ++j) {
        out_leaves.push_back(old_leaves[j]);
      }
    };

    size_t new_total_size = 0;
    for (; leaf_idx < old_leaves.size(); ++leaf_idx) {
      LeafNode* leaf = old_leaves[leaf_idx];
      size_t old_count = leaf->num_keys;
      const Key& first_k = leaf->key(0);
      while (rhs_it != rhs_end && rhs_it->first < first_k) {
        ++rhs_it;
      }
      if (rhs_it != rhs_end && rhs_it.CurrentLeaf() == leaf &&
          CanSkipSharedRange<kNeverKeepLhsOnly, kIsDefaultMerge>(
              leaf->entries(), leaf->entries() + old_count, merge)) {
        DCHECK_EQ(rhs_it.CurrentIndex(), 0);
        rhs_it.SkipCurrentLeaf();
        new_total_size += old_count;
        if (tree_detached) {
          AppendOrCoalesceLeaf(out_leaves, leaf);
        }
        continue;
      }
      size_t write_idx = process_leaf(
          leaf, tree_detached && (leaf->ref_count == 1), [&](LeafNode*& l) {
            ensure_detached();
            if (l->ref_count > 1) l = UnshareLeaf(l);
          });
      if (write_idx == 0) {
        ensure_detached();
        Release(leaf);
        continue;
      }
      if (write_idx < old_count) {
        for (size_t i = write_idx; i < old_count; ++i) {
          leaf->entries()[i].~value_type();
        }
        leaf->num_keys = static_cast<uint16_t>(write_idx);
      }
      new_total_size += write_idx;
      if (tree_detached) {
        AppendOrCoalesceLeaf(out_leaves, leaf);
      }
    }

    if (!tree_detached) return;
    BuildTreeFromLeaves(out_leaves, new_total_size);
  }

  // Returns true iff every entry `(k, a_val)` in `*this` (excluding entries
  // where `is_empty(a_val)` is true) has a matching key `k` in `bs` with
  // `compare(a_val, b_val)` returning true. Requires `compare(v, v) == true`
  // (reflexivity) so shared subtrees and leaves can be skipped in O(1).
  template <typename CompareFunction, typename IsEmptyFunction = std::nullptr_t>
  bool Includes(const KnaMap& bs, const CompareFunction& compare,
                const IsEmptyFunction is_empty = nullptr) const {
    if (IsSharedWith(bs)) return true;
    const_iterator a = begin();
    const_iterator b = bs.begin();
    while (a != end()) {
      if (b != bs.end() && a.CurrentLeaf() == b.CurrentLeaf() &&
          a.CurrentIndex() == b.CurrentIndex()) {
        a.SkipCurrentLeaf();
        b.SkipCurrentLeaf();
        continue;
      }
      if constexpr (!std::is_same_v<IsEmptyFunction, std::nullptr_t>) {
        if (is_empty(a->second)) {
          ++a;
          continue;
        }
      }
      if (b == bs.end()) return false;
      while (b->first < a->first) {
        ++b;
        if (b == bs.end()) return false;
      }
      if (!(a->first == b->first)) return false;
      if (!compare(a->second, b->second)) {
        return false;
      }
      ++a;
      ++b;
    }
    return true;
  }

 private:
  struct TreeNodeBase {
    uint32_t ref_count;
    uint16_t num_keys;
    bool is_leaf;
  };

  struct LeafNode : public TreeNodeBase {
    union {
      alignas(value_type) char storage[sizeof(value_type) * kMaxKeys];
      LeafNode* next_free;
    };

    value_type* entries() {
      return std::launder(reinterpret_cast<value_type*>(storage));
    }
    const value_type* entries() const {
      return std::launder(reinterpret_cast<const value_type*>(storage));
    }
    const Key& key(size_t i) const { return entries()[i].first; }
    Value& value(size_t i) { return entries()[i].second; }
    const Value& value(size_t i) const { return entries()[i].second; }
  };

  struct InternalNode : public TreeNodeBase {
    alignas(Key) char key_storage[sizeof(Key) * kMaxKeys];
    union {
      TreeNodeBase* children[kMaxChildren];
      InternalNode* next_free;
    };

    Key* keys() { return std::launder(reinterpret_cast<Key*>(key_storage)); }
    const Key* keys() const {
      return std::launder(reinterpret_cast<const Key*>(key_storage));
    }
  };

  struct PathEntry {
    InternalNode* parent;
    uint16_t child_idx;
  };

  bool IsSharedWith(const KnaMap& other) const { return root_ == other.root_; }

  // LINT.IfChange(KnaMergeSkipShared)
  template <bool kNeverKeepLhsOnly, bool kIsDefaultMerge, typename Iter,
            typename MergeFunc>
  static bool CanSkipSharedRange(Iter first, Iter last, MergeFunc& merge) {
    if constexpr (!kNeverKeepLhsOnly) {
      return false;
    } else if constexpr (kIsDefaultMerge || std::is_same_v<Value, ValueNode*>) {
      return true;
    } else if constexpr (requires {
                           first->second.IsSharedWith(first->second);
                         }) {
      // `LoadedPropertyMap` (`KnaMap<PropertyKey, PropertyObjectMap>`) retains
      // an entry only if its merged inner map is non-empty, while
      // `ClearLoadedPropertiesForKey` clears an inner map in place without
      // erasing the outer entry. Only skip the merge if every inner map is
      // non-empty.
      for (Iter it = first; it != last; ++it) {
        if (it->second.empty()) return false;
      }
      return true;
    } else if constexpr (requires { first->second.no_info_available(); }) {
      // When two control-flow paths share the exact same `node_infos_` tree or
      // leaf, merging any entry `(k, v)` with itself is idempotent and retains
      // the entry unless `v.no_info_available()` is true. However,
      // `KnownNodeAspects::Merge` has a side-effecting callback on the merge
      // lambda (`side_effects_require_invalidation_ |= ...`) whenever an entry
      // has unstable non-stale maps, so we must invoke `merge` on at least one
      // such entry to preserve that flag.
      if (V8_UNLIKELY(v8_flags.trace_maglev_kna)) return false;
      const value_type* unstable_entry = nullptr;
      for (Iter it = first; it != last; ++it) {
        if (it->second.no_info_available()) return false;
        if (!unstable_entry && it->second.any_map_is_unstable() &&
            !it->second.maps_are_stale()) {
          unstable_entry = &(*it);
        }
      }
      if (unstable_entry) {
        Value temp_val = unstable_entry->second;
        merge(unstable_entry->first, temp_val, unstable_entry->second);
      }
      return true;
    } else if constexpr (requires { first->second.effect_epoch; }) {
      for (Iter it = first; it != last; ++it) {
        Value temp_val = it->second;
        if (!merge(it->first, temp_val, it->second)) return false;
      }
      return true;
    } else {
      return false;
    }
  }
  // LINT.ThenChange(maglev-known-node-aspects.cc:KnaMergeSkipShared)

  // Tests whether a tentatively mutated copy `a` (initialized from `b`) is
  // identical to `b`, allowing `EraseIf`, `MutateValues`, and
  // `DestructivelyMergeWith` to avoid unsharing a leaf when a callback leaves
  // the value unchanged.
  static bool ValueEquals(const Value& a, const Value& b) {
    if constexpr (requires { a.IsSharedWith(b); }) {
      // `a` was copy-constructed from `b` before invoking the callback; if the
      // callback did not mutate `a`, `a.root_ == b.root_`.
      return a.IsSharedWith(b);
    } else {
      return a == b;
    }
  }

  static void SetKey(InternalNode* node, size_t i, const Key& k) {
    new (&node->keys()[i]) Key(k);
  }

  static size_t FindChildIndex(const InternalNode* node, const Key& k) {
    size_t i = 0;
    size_t count = node->num_keys;
    const Key* keys = node->keys();
    while (i < count && !(k < keys[i])) {
      ++i;
    }
    return i;
  }

  static size_t LowerBound(const LeafNode* leaf, const Key& k) {
    size_t i = 0;
    size_t count = leaf->num_keys;
    const value_type* entries = leaf->entries();
    while (i < count && entries[i].first < k) {
      ++i;
    }
    return i;
  }

  static const Key& MinKey(const TreeNodeBase* node) {
    while (!node->is_leaf) {
      node = static_cast<const InternalNode*>(node)->children[0];
    }
    return static_cast<const LeafNode*>(node)->key(0);
  }

  LeafNode* AllocLeaf() {
    static_assert(alignof(LeafNode) <= 8);
    static_assert(sizeof(LeafNode::storage) >= sizeof(LeafNode*));
    DCHECK_NOT_NULL(pool_);
    LeafNode* node;
    if (pool_->free_leaves) {
      node = pool_->free_leaves;
      pool_->free_leaves = node->next_free;
      ASAN_UNPOISON_MEMORY_REGION(
          reinterpret_cast<char*>(&node->next_free) + sizeof(node->next_free),
          sizeof(node->storage) - sizeof(node->next_free));
    } else {
      node = static_cast<LeafNode*>(
          pool_->zone->template Allocate<LeafNode>(sizeof(LeafNode)));
    }
    node->ref_count = 1;
    node->num_keys = 0;
    node->is_leaf = true;
    return node;
  }

  InternalNode* AllocInternal() {
    static_assert(alignof(InternalNode) <= 8);
    static_assert(sizeof(InternalNode::children) >= sizeof(InternalNode*));
    DCHECK_NOT_NULL(pool_);
    InternalNode* node;
    if (pool_->free_internals) {
      node = pool_->free_internals;
      pool_->free_internals = node->next_free;
      ASAN_UNPOISON_MEMORY_REGION(&node->key_storage[0],
                                  sizeof(node->key_storage));
      ASAN_UNPOISON_MEMORY_REGION(
          reinterpret_cast<char*>(&node->next_free) + sizeof(node->next_free),
          sizeof(node->children) - sizeof(node->next_free));
    } else {
      node = static_cast<InternalNode*>(
          pool_->zone->template Allocate<InternalNode>(sizeof(InternalNode)));
    }
    node->ref_count = 1;
    node->num_keys = 0;
    node->is_leaf = false;
    return node;
  }

  static void AddRef(TreeNodeBase* node) {
    if (node) ++node->ref_count;
  }

  void Release(TreeNodeBase* node) {
    if (!node) return;
    DCHECK_GT(node->ref_count, 0u);
    if (--node->ref_count > 0) return;
    if (node->is_leaf) {
      LeafNode* leaf = static_cast<LeafNode*>(node);
      value_type* entries = leaf->entries();
      for (size_t i = 0, n = leaf->num_keys; i < n; ++i) {
        entries[i].~value_type();
      }
      leaf->next_free = pool_->free_leaves;
      pool_->free_leaves = leaf;
      ASAN_POISON_MEMORY_REGION(
          reinterpret_cast<char*>(&leaf->next_free) + sizeof(leaf->next_free),
          sizeof(leaf->storage) - sizeof(leaf->next_free));
    } else {
      InternalNode* internal = static_cast<InternalNode*>(node);
      for (size_t i = 0, n = internal->num_keys; i <= n; ++i) {
        Release(internal->children[i]);
      }
      internal->next_free = pool_->free_internals;
      pool_->free_internals = internal;
      ASAN_POISON_MEMORY_REGION(&internal->key_storage[0],
                                sizeof(internal->key_storage));
      ASAN_POISON_MEMORY_REGION(
          reinterpret_cast<char*>(&internal->next_free) +
              sizeof(internal->next_free),
          sizeof(internal->children) - sizeof(internal->next_free));
    }
  }

  LeafNode* UnshareLeaf(LeafNode* old_leaf) {
    DCHECK_GT(old_leaf->ref_count, 1u);
    LeafNode* new_leaf = AllocLeaf();
    size_t count = old_leaf->num_keys;
    new_leaf->num_keys = static_cast<uint16_t>(count);
    const value_type* src = old_leaf->entries();
    value_type* dst = new_leaf->entries();
    for (size_t i = 0; i < count; ++i) {
      new (&dst[i]) value_type(src[i]);
    }
    --old_leaf->ref_count;
    return new_leaf;
  }

  InternalNode* UnshareInternal(InternalNode* old_internal) {
    DCHECK_GT(old_internal->ref_count, 1u);
    InternalNode* new_internal = AllocInternal();
    size_t count = old_internal->num_keys;
    new_internal->num_keys = static_cast<uint16_t>(count);
    const Key* src_keys = old_internal->keys();
    for (size_t i = 0; i < count; ++i) {
      SetKey(new_internal, i, src_keys[i]);
    }
    for (size_t i = 0; i <= count; ++i) {
      TreeNodeBase* child = old_internal->children[i];
      AddRef(child);
      new_internal->children[i] = child;
    }
    --old_internal->ref_count;
    return new_internal;
  }

  LeafNode* UnsharePathToLeaf(PathEntry* path, size_t depth, LeafNode* leaf) {
    if (depth == 0) {
      if (leaf->ref_count > 1) {
        leaf = UnshareLeaf(leaf);
        root_ = leaf;
      }
      return leaf;
    }
    if (path[0].parent->ref_count > 1) {
      path[0].parent = UnshareInternal(path[0].parent);
      root_ = path[0].parent;
    }
    for (size_t d = 1; d < depth; ++d) {
      if (path[d].parent->ref_count > 1) {
        path[d].parent = UnshareInternal(path[d].parent);
        path[d - 1].parent->children[path[d - 1].child_idx] = path[d].parent;
      }
    }
    if (leaf->ref_count > 1) {
      leaf = UnshareLeaf(leaf);
      path[depth - 1].parent->children[path[depth - 1].child_idx] = leaf;
    }
    return leaf;
  }

  const_iterator FindConstIterator(const Key& k) const {
    if (!root_) return const_iterator();
    const TreeNodeBase* curr = root_;
    while (!curr->is_leaf) {
      const InternalNode* internal = static_cast<const InternalNode*>(curr);
      curr = internal->children[FindChildIndex(internal, k)];
    }
    const LeafNode* leaf = static_cast<const LeafNode*>(curr);
    size_t pos = LowerBound(leaf, k);
    if (pos < leaf->num_keys && leaf->key(pos) == k) {
      return const_iterator(leaf, static_cast<uint16_t>(pos));
    }
    return const_iterator();
  }

  const value_type* FindEntry(const Key& k) const {
    const_iterator it = FindConstIterator(k);
    return it != end() ? &(*it) : nullptr;
  }

  value_type* FindMutableEntry(const Key& k) {
    if (!root_) return nullptr;
    PathEntry path[kMaxDepth];
    size_t depth = 0;
    TreeNodeBase* curr = root_;
    while (!curr->is_leaf) {
      InternalNode* internal = static_cast<InternalNode*>(curr);
      size_t idx = FindChildIndex(internal, k);
      DCHECK_LT(depth, kMaxDepth);
      path[depth++] = {internal, static_cast<uint16_t>(idx)};
      curr = internal->children[idx];
    }
    LeafNode* leaf = static_cast<LeafNode*>(curr);
    size_t pos = LowerBound(leaf, k);
    if (pos == leaf->num_keys || !(leaf->key(pos) == k)) {
      return nullptr;
    }
    leaf = UnsharePathToLeaf(path, depth, leaf);
    return &leaf->entries()[pos];
  }

  template <typename... Args>
  static value_type* InsertIntoNonFullLeaf(LeafNode* leaf, size_t pos,
                                           const Key& k, Args&&... args) {
    DCHECK_LT(leaf->num_keys, kMaxKeys);
    value_type* entries = leaf->entries();
    size_t count = leaf->num_keys;
    if (pos < count) {
      new (&entries[count]) value_type(std::move(entries[count - 1]));
      for (size_t i = count - 1; i > pos; --i) {
        entries[i] = std::move(entries[i - 1]);
      }
      entries[pos].~value_type();
    }
    value_type* res = new (&entries[pos])
        value_type(std::piecewise_construct, std::forward_as_tuple(k),
                   std::forward_as_tuple(std::forward<Args>(args)...));
    leaf->num_keys = static_cast<uint16_t>(count + 1);
    return res;
  }

  template <typename... Args>
  std::pair<value_type*, bool> InsertImpl(const Key& k, Args&&... args) {
    if (!root_) {
      LeafNode* leaf = AllocLeaf();
      value_type* res =
          InsertIntoNonFullLeaf(leaf, 0, k, std::forward<Args>(args)...);
      root_ = leaf;
      size_ = 1;
      return {res, true};
    }

    PathEntry path[kMaxDepth];
    size_t depth = 0;
    TreeNodeBase* curr = root_;
    while (!curr->is_leaf) {
      InternalNode* internal = static_cast<InternalNode*>(curr);
      size_t idx = FindChildIndex(internal, k);
      DCHECK_LT(depth, kMaxDepth);
      path[depth++] = {internal, static_cast<uint16_t>(idx)};
      curr = internal->children[idx];
    }
    LeafNode* leaf = static_cast<LeafNode*>(curr);
    size_t pos = LowerBound(leaf, k);
    leaf = UnsharePathToLeaf(path, depth, leaf);
    if (pos < leaf->num_keys && leaf->key(pos) == k) {
      return {&leaf->entries()[pos], false};
    }

    ++size_;
    if (leaf->num_keys < kMaxKeys) {
      return {InsertIntoNonFullLeaf(leaf, pos, k, std::forward<Args>(args)...),
              true};
    }

    // Split full leaf into `leaf` (left) and `right`.
    LeafNode* right = AllocLeaf();
    value_type* inserted_ptr = nullptr;
    value_type* left_entries = leaf->entries();
    value_type* right_entries = right->entries();
    if (pos <= kMinKeys) {
      for (size_t i = 0; i < kMinKeys; ++i) {
        new (&right_entries[i])
            value_type(std::move(left_entries[kMinKeys + i]));
        left_entries[kMinKeys + i].~value_type();
      }
      right->num_keys = static_cast<uint16_t>(kMinKeys);
      leaf->num_keys = static_cast<uint16_t>(kMinKeys);
      inserted_ptr =
          InsertIntoNonFullLeaf(leaf, pos, k, std::forward<Args>(args)...);
    } else {
      constexpr size_t kSplit = kMinKeys + 1;
      constexpr size_t kRightInitial = kMaxKeys - kSplit;
      for (size_t i = 0; i < kRightInitial; ++i) {
        new (&right_entries[i]) value_type(std::move(left_entries[kSplit + i]));
        left_entries[kSplit + i].~value_type();
      }
      leaf->num_keys = static_cast<uint16_t>(kSplit);
      right->num_keys = static_cast<uint16_t>(kRightInitial);
      inserted_ptr = InsertIntoNonFullLeaf(right, pos - kSplit, k,
                                           std::forward<Args>(args)...);
    }

    Key up_key = right->key(0);
    TreeNodeBase* up_child = right;

    for (int d = static_cast<int>(depth) - 1; d >= 0; --d) {
      InternalNode* parent = path[d].parent;
      size_t idx = path[d].child_idx;
      size_t p_count = parent->num_keys;
      if (p_count < kMaxKeys) {
        Key* p_keys = parent->keys();
        for (size_t i = p_count; i > idx; --i) {
          SetKey(parent, i, p_keys[i - 1]);
          parent->children[i + 1] = parent->children[i];
        }
        SetKey(parent, idx, up_key);
        parent->children[idx + 1] = up_child;
        parent->num_keys = static_cast<uint16_t>(p_count + 1);
        return {inserted_ptr, true};
      }

      // Split full InternalNode `parent`.
      alignas(Key) char tmp_keys_buf[sizeof(Key) * (kMaxKeys + 1)];
      Key* tmp_keys = std::launder(reinterpret_cast<Key*>(tmp_keys_buf));
      TreeNodeBase* tmp_children[kMaxKeys + 2];

      const Key* p_keys = parent->keys();
      for (size_t i = 0; i < idx; ++i) {
        new (&tmp_keys[i]) Key(p_keys[i]);
        tmp_children[i] = parent->children[i];
      }
      tmp_children[idx] = parent->children[idx];
      new (&tmp_keys[idx]) Key(up_key);
      tmp_children[idx + 1] = up_child;
      for (size_t i = idx; i < kMaxKeys; ++i) {
        new (&tmp_keys[i + 1]) Key(p_keys[i]);
        tmp_children[i + 2] = parent->children[i + 1];
      }

      parent->num_keys = static_cast<uint16_t>(kMinKeys);
      for (size_t i = 0; i < kMinKeys; ++i) {
        SetKey(parent, i, tmp_keys[i]);
        parent->children[i] = tmp_children[i];
      }
      parent->children[kMinKeys] = tmp_children[kMinKeys];

      InternalNode* right_internal = AllocInternal();
      right_internal->num_keys = static_cast<uint16_t>(kMinKeys);
      for (size_t i = 0; i < kMinKeys; ++i) {
        SetKey(right_internal, i, tmp_keys[kMinKeys + 1 + i]);
        right_internal->children[i] = tmp_children[kMinKeys + 1 + i];
      }
      right_internal->children[kMinKeys] = tmp_children[kMaxKeys + 1];

      up_key = tmp_keys[kMinKeys];
      up_child = right_internal;
    }

    CHECK_LT(depth + 1, kMaxDepth);
    InternalNode* new_root = AllocInternal();
    new_root->num_keys = 1;
    SetKey(new_root, 0, up_key);
    new_root->children[0] = root_;
    new_root->children[1] = up_child;
    root_ = new_root;
    return {inserted_ptr, true};
  }

  template <typename MutateFn>
  void MutateValuesRec(TreeNodeBase* curr, PathEntry* path, size_t depth,
                       bool path_unshared, MutateFn& fn) {
    if (curr->is_leaf) {
      LeafNode* leaf = static_cast<LeafNode*>(curr);
      bool unshared = path_unshared && (leaf->ref_count == 1);
      for (size_t i = 0, n = leaf->num_keys; i < n; ++i) {
        if (!unshared) {
          Value temp_val = leaf->value(i);
          fn(leaf->key(i), temp_val);
          if (ValueEquals(temp_val, leaf->value(i))) {
            continue;
          }
          leaf = UnsharePathToLeaf(path, depth, leaf);
          unshared = true;
          leaf->value(i) = std::move(temp_val);
          continue;
        }
        fn(leaf->key(i), leaf->value(i));
      }
      return;
    }
    DCHECK_LT(depth, kMaxDepth);
    InternalNode* internal = static_cast<InternalNode*>(curr);
    size_t count = internal->num_keys;
    for (size_t i = 0; i <= count; ++i) {
      path[depth] = {internal, static_cast<uint16_t>(i)};
      MutateValuesRec(internal->children[i], path, depth + 1,
                      path_unshared && (internal->ref_count == 1), fn);
      // Reload `internal` in case a descendant unshared the path; if so, the
      // path down to `internal` is now exclusively owned (`ref_count == 1`).
      if (path[depth].parent != internal) {
        internal = path[depth].parent;
        path_unshared = true;
      }
    }
  }

  static void CollectLeaves(TreeNodeBase* node,
                            base::SmallVector<LeafNode*, 64>& out) {
    if (node->is_leaf) {
      out.push_back(static_cast<LeafNode*>(node));
      return;
    }
    InternalNode* internal = static_cast<InternalNode*>(node);
    for (size_t i = 0, n = internal->num_keys; i <= n; ++i) {
      CollectLeaves(internal->children[i], out);
    }
  }

  void AppendOrCoalesceLeaf(base::SmallVector<LeafNode*, 64>& out_leaves,
                            LeafNode* leaf) {
    if (out_leaves.empty()) {
      out_leaves.push_back(leaf);
      return;
    }
    LeafNode* prev = out_leaves.back();
    if (prev->num_keys >= kMinKeys && leaf->num_keys >= kMinKeys) {
      out_leaves.push_back(leaf);
      return;
    }
    if (prev->ref_count > 1) {
      prev = UnshareLeaf(prev);
      out_leaves.back() = prev;
    }
    size_t prev_count = prev->num_keys;
    size_t leaf_count = leaf->num_keys;
    size_t total = prev_count + leaf_count;
    value_type* prev_entries = prev->entries();
    if (total <= kMaxKeys) {
      if (leaf->ref_count == 1) {
        value_type* leaf_entries = leaf->entries();
        for (size_t i = 0; i < leaf_count; ++i) {
          new (&prev_entries[prev_count + i])
              value_type(std::move(leaf_entries[i]));
        }
      } else {
        const value_type* leaf_entries = leaf->entries();
        for (size_t i = 0; i < leaf_count; ++i) {
          new (&prev_entries[prev_count + i]) value_type(leaf_entries[i]);
        }
      }
      prev->num_keys = static_cast<uint16_t>(total);
      Release(leaf);
      return;
    }
    if (leaf->ref_count > 1) {
      leaf = UnshareLeaf(leaf);
    }
    value_type* leaf_entries = leaf->entries();
    size_t target_prev = total / 2;
    if (prev_count < target_prev) {
      size_t move_count = target_prev - prev_count;
      for (size_t i = 0; i < move_count; ++i) {
        new (&prev_entries[prev_count + i])
            value_type(std::move(leaf_entries[i]));
      }
      for (size_t i = 0; i < leaf_count - move_count; ++i) {
        leaf_entries[i] = std::move(leaf_entries[move_count + i]);
      }
      for (size_t i = leaf_count - move_count; i < leaf_count; ++i) {
        leaf_entries[i].~value_type();
      }
      prev->num_keys = static_cast<uint16_t>(target_prev);
      leaf->num_keys = static_cast<uint16_t>(leaf_count - move_count);
    } else if (prev_count > target_prev) {
      size_t move_count = prev_count - target_prev;
      for (size_t i = leaf_count + move_count; i > move_count; --i) {
        size_t dst_idx = i - 1;
        size_t src_idx = dst_idx - move_count;
        if (dst_idx >= leaf_count) {
          new (&leaf_entries[dst_idx])
              value_type(std::move(leaf_entries[src_idx]));
        } else {
          leaf_entries[dst_idx] = std::move(leaf_entries[src_idx]);
        }
      }
      for (size_t i = 0; i < move_count; ++i) {
        if (i >= leaf_count) {
          new (&leaf_entries[i])
              value_type(std::move(prev_entries[target_prev + i]));
        } else {
          leaf_entries[i] = std::move(prev_entries[target_prev + i]);
        }
        prev_entries[target_prev + i].~value_type();
      }
      prev->num_keys = static_cast<uint16_t>(target_prev);
      leaf->num_keys = static_cast<uint16_t>(leaf_count + move_count);
    }
    out_leaves.push_back(leaf);
  }

  void BuildTreeFromLeaves(base::SmallVector<LeafNode*, 64>& leaves,
                           size_t total_size) {
    if (leaves.empty()) {
      root_ = nullptr;
      size_ = 0;
      return;
    }
    size_ = static_cast<uint32_t>(total_size);
    if (leaves.size() == 1) {
      root_ = leaves[0];
      return;
    }
    base::SmallVector<TreeNodeBase*, 64> current_level;
    current_level.reserve(leaves.size());
    for (LeafNode* l : leaves) current_level.push_back(l);

    while (current_level.size() > 1) {
      base::SmallVector<TreeNodeBase*, 64> next_level;
      size_t m = current_level.size();
      size_t idx = 0;
      while (idx < m) {
        size_t remaining = m - idx;
        size_t group_size;
        if (remaining <= kMaxChildren) {
          group_size = remaining;
        } else if (remaining <= kMaxChildren + kMinKeys + 1) {
          // Avoid leaving a trailing group with fewer than `kMinKeys + 1`
          // children by splitting the last two groups evenly.
          group_size = remaining / 2;
        } else {
          group_size = kMaxChildren;
        }
        InternalNode* parent = AllocInternal();
        parent->num_keys = static_cast<uint16_t>(group_size - 1);
        parent->children[0] = current_level[idx];
        for (size_t j = 1; j < group_size; ++j) {
          TreeNodeBase* child = current_level[idx + j];
          parent->children[j] = child;
          SetKey(parent, j - 1, MinKey(child));
        }
        next_level.push_back(parent);
        idx += group_size;
      }
      current_level = std::move(next_level);
    }
    root_ = current_level[0];
  }

  TreeNodeBase* root_ = nullptr;
  Pool* pool_ = nullptr;
  uint32_t size_ = 0;
};

#else  // !V8_ENABLE_MAGLEV_KNA_BTREE

// Thin shim around ZoneMap<Key, Value> providing the KnaMap interface
// used by KnownNodeAspects.
template <typename Key, typename Value>
class KnaMap {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = typename ZoneMap<Key, Value>::value_type;
  using iterator = typename ZoneMap<Key, Value>::iterator;
  using const_iterator = typename ZoneMap<Key, Value>::const_iterator;

  struct Pool {
    explicit Pool(Zone* z) : zone(z) {}
    Zone* zone = nullptr;
  };

  explicit KnaMap(Pool* pool) : map_(pool->zone) {}
  KnaMap(const KnaMap& other) V8_NOEXCEPT = default;
  template <typename Func>
  KnaMap(const KnaMap& other, Func&& mutate) : KnaMap(other) {
    MutateValues(std::forward<Func>(mutate));
  }
  KnaMap(KnaMap&& other) V8_NOEXCEPT = default;
  KnaMap& operator=(const KnaMap& other) V8_NOEXCEPT = default;
  KnaMap& operator=(KnaMap&& other) V8_NOEXCEPT = default;
  ~KnaMap() = default;

  Zone* zone() const { return map_.get_allocator().zone(); }

  bool empty() const { return map_.empty(); }
  size_t size() const { return map_.size(); }

  void clear() { map_.clear(); }

  const_iterator begin() const { return map_.begin(); }
  const_iterator end() const { return map_.end(); }

  const_iterator find(const Key& k) const { return map_.find(k); }
  iterator find(const Key& k) { return map_.find(k); }
  bool contains(const Key& k) const { return map_.contains(k); }

  Value& operator[](const Key& k) { return map_[k]; }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
    return map_.try_emplace(k, std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    return map_.emplace(std::forward<Args>(args)...);
  }

  template <typename PairT = value_type>
  std::pair<iterator, bool> insert(PairT&& entry) {
    return map_.insert(std::forward<PairT>(entry));
  }

  size_t erase(const Key& k) { return map_.erase(k); }

  // Visits entries in ascending key order, erasing those for which
  // `pred(const Key&, Value&)` returns true. `pred` may mutate `Value&` in
  // place for surviving entries.
  template <typename Pred>
  size_t EraseIf(Pred&& pred) {
    return std::erase_if(map_, [&](value_type& entry) {
      return pred(entry.first, entry.second);
    });
  }

  // Visits all entries in ascending key order, allowing `fn(const Key&,
  // Value&)` to mutate `Value&` in place without changing keys or erasing
  // entries.
  template <typename MutateFn>
  void MutateValues(MutateFn&& fn) {
    for (auto& [k, v] : map_) {
      fn(k, v);
    }
  }

  // Rebuilds the map by applying `remap_key(const Key&)` to each key. If
  // multiple keys map to the same normalized key, `combine(Value& dst, const
  // Value& src)` merges subsequent colliding values into the first inserted
  // value (visited in ascending original-key order).
  template <typename RemapKeyFn, typename CombineFn>
  void RemapKeys(RemapKeyFn&& remap_key, CombineFn&& combine) {
    ZoneMap<Key, Value> remapped(zone());
    for (const auto& [k, v] : map_) {
      auto [it, inserted] = remapped.try_emplace(remap_key(k), v);
      if (!inserted) {
        combine(it->second, v);
      }
    }
    map_ = std::move(remapped);
  }

  template <typename RemapKeyFn>
  void RemapKeys(RemapKeyFn&& remap_key) {
    RemapKeys(std::forward<RemapKeyFn>(remap_key),
              [](Value& dst, const Value& src) { dst = src; });
  }

  // Destructively merges `rhs_map` into `*this` in ascending key order:
  // - Keys in both maps: `merge(const Key&, Value& lhs, const Value& rhs)` may
  //   mutate `lhs` and returns true to keep the entry, or false to erase it.
  //   For `Value == ValueNode*`, if `merge` returns false and either value is
  //   an Identity wrapper, `lhs` is updated to `lhs->UnwrapIdentities()` and
  //   `merge` is retried with the unwrapped values.
  // - Keys only in `*this`: `keep_lhs_only(const Key&, Value& lhs)` may mutate
  //   `lhs` and returns true to keep the entry, or false to erase it.
  // - Keys only in `rhs_map`: dropped.
  template <typename MergeFunc, typename KeepLhsOnlyFunc>
  void DestructivelyMergeWith(const KnaMap& rhs_map, MergeFunc&& merge,
                              KeepLhsOnlyFunc&& keep_lhs_only) {
    DCHECK_NE(this, &rhs_map);
    auto lhs_it = map_.begin();
    auto rhs_it = rhs_map.map_.begin();
    while (lhs_it != map_.end() && rhs_it != rhs_map.map_.end()) {
      if (lhs_it->first < rhs_it->first) {
        if (keep_lhs_only(lhs_it->first, lhs_it->second)) {
          ++lhs_it;
        } else {
          lhs_it = map_.erase(lhs_it);
        }
      } else if (rhs_it->first < lhs_it->first) {
        ++rhs_it;
      } else {
        bool keep_value = merge(lhs_it->first, lhs_it->second, rhs_it->second);
        if constexpr (std::is_same_v<Value, ValueNode*>) {
          if (!keep_value && lhs_it->second && rhs_it->second) {
            auto l = lhs_it->second->UnwrapIdentities();
            auto r = rhs_it->second->UnwrapIdentities();
            if (l != lhs_it->second || r != rhs_it->second) {
              lhs_it->second = l;
              keep_value = merge(lhs_it->first, lhs_it->second, r);
            }
          }
        }
        if (keep_value) {
          ++lhs_it;
        } else {
          lhs_it = map_.erase(lhs_it);
        }
        ++rhs_it;
      }
    }
    if constexpr (std::is_same_v<std::decay_t<KeepLhsOnlyFunc>,
                                 NeverKeepLhsOnly>) {
      if (lhs_it != map_.end()) {
        map_.erase(lhs_it, map_.end());
      }
    } else {
      while (lhs_it != map_.end()) {
        if (keep_lhs_only(lhs_it->first, lhs_it->second)) {
          ++lhs_it;
        } else {
          lhs_it = map_.erase(lhs_it);
        }
      }
    }
  }

  // Returns true iff every entry `(k, a_val)` in `*this` (excluding entries
  // where `is_empty(a_val)` is true) has a matching key `k` in `bs` with
  // `compare(a_val, b_val)` returning true.
  template <typename CompareFunction, typename IsEmptyFunction = std::nullptr_t>
  bool Includes(const KnaMap& bs, const CompareFunction& compare,
                const IsEmptyFunction is_empty = nullptr) const {
    const_iterator a = map_.begin();
    const_iterator b = bs.map_.begin();
    while (a != map_.end()) {
      if constexpr (!std::is_same_v<IsEmptyFunction, std::nullptr_t>) {
        if (is_empty(a->second)) {
          ++a;
          continue;
        }
      }
      if (b == bs.map_.end()) return false;
      while (b->first < a->first) {
        ++b;
        if (b == bs.map_.end()) return false;
      }
      if (!(a->first == b->first)) return false;
      if (!compare(a->second, b->second)) {
        return false;
      }
      ++a;
      ++b;
    }
    return true;
  }

 private:
  ZoneMap<Key, Value> map_;
};

#endif  // V8_ENABLE_MAGLEV_KNA_BTREE

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_KNA_MAP_H_
