// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_KNA_MAP_H_
#define V8_MAGLEV_MAGLEV_KNA_MAP_H_

#include <cstddef>
#include <type_traits>
#include <utility>

#include "src/base/compiler-specific.h"
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
  bool operator()(const Key&, const Value&) const {
    return false;
  }
};

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

  explicit KnaMap(Zone* zone) : map_(zone) {}
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
  iterator erase(const_iterator it) { return map_.erase(it); }

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

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_KNA_MAP_H_
