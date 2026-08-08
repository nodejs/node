// Copyright 2025 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: linked_hash_set.h
// -----------------------------------------------------------------------------
//
// This is a simple insertion-ordered set. It provides O(1) amortized
// insertions and lookups, as well as iteration over the set in the insertion
// order.
//
// This class is thread-compatible.
//
// Iterators point into the list and should be stable in the face of
// mutations, except for an iterator pointing to an element that was just
// deleted.
//
// This class supports heterogeneous lookups.

#ifndef ABSL_CONTAINER_LINKED_HASH_SET_H_
#define ABSL_CONTAINER_LINKED_HASH_SET_H_

#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <list>
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/internal/common.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

template <
    typename Key, typename KeyHash = typename absl::flat_hash_set<Key>::hasher,
    typename KeyEq = typename absl::flat_hash_set<Key, KeyHash>::key_equal,
    typename Alloc = std::allocator<Key>>
class linked_hash_set {
  using KeyArgImpl = absl::container_internal::KeyArg<
      absl::container_internal::IsTransparent<KeyEq>::value &&
      absl::container_internal::IsTransparent<KeyHash>::value>;

 public:
  using key_type = Key;
  using hasher = KeyHash;
  using key_equal = KeyEq;
  using value_type = key_type;
  using allocator_type = Alloc;
  using difference_type = ptrdiff_t;

 private:
  template <class K>
  using key_arg = typename KeyArgImpl::template type<K, key_type>;

  using ListType = std::list<key_type, Alloc>;

  template <class Fn>
  class Wrapped {
    template <typename K>
    static const K& ToKey(const K& k) {
      return k;
    }
    static const key_type& ToKey(typename ListType::const_iterator it) {
      return *it;
    }
    static const key_type& ToKey(typename ListType::iterator it) { return *it; }

    Fn fn_;

    friend linked_hash_set;

   public:
    using is_transparent = void;

    Wrapped() = default;
    explicit Wrapped(Fn fn) : fn_(std::move(fn)) {}

    template <class... Args>
    auto operator()(Args&&... args) const
        -> decltype(this->fn_(ToKey(args)...)) {
      return fn_(ToKey(args)...);
    }
  };
  using SetType =
      absl::flat_hash_set<typename ListType::iterator, Wrapped<hasher>,
                          Wrapped<key_equal>, Alloc>;

  class NodeHandle {
   public:
    using allocator_type = linked_hash_set::allocator_type;
    using value_type = linked_hash_set::value_type;

    constexpr NodeHandle() noexcept = default;
    NodeHandle(NodeHandle&& nh) noexcept = default;
    ~NodeHandle() = default;
    NodeHandle& operator=(NodeHandle&& node) noexcept = default;
    bool empty() const noexcept { return list_.empty(); }
    explicit operator bool() const noexcept { return !empty(); }
    allocator_type get_allocator() const { return list_.get_allocator(); }
    value_type& value() { return list_.front(); }
    void swap(NodeHandle& nh) noexcept { list_.swap(nh.list_); }

   private:
    friend linked_hash_set;

    explicit NodeHandle(ListType list) : list_(std::move(list)) {}
    ListType list_;
  };

  template <class Iterator, class NodeType>
  struct InsertReturnType {
    Iterator position;
    bool inserted;
    NodeType node;
  };

 public:
  using iterator = typename ListType::const_iterator;
  using const_iterator = typename ListType::const_iterator;
  using reverse_iterator = typename ListType::const_reverse_iterator;
  using const_reverse_iterator = typename ListType::const_reverse_iterator;
  using reference = typename ListType::reference;
  using const_reference = typename ListType::const_reference;
  using pointer = typename std::allocator_traits<allocator_type>::pointer;
  using const_pointer =
      typename std::allocator_traits<allocator_type>::const_pointer;
  using size_type = typename ListType::size_type;
  using node_type = NodeHandle;
  using insert_return_type = InsertReturnType<iterator, node_type>;

  linked_hash_set() {}

  explicit linked_hash_set(size_t bucket_count, const hasher& hash = hasher(),
                           const key_equal& eq = key_equal(),
                           const allocator_type& alloc = allocator_type())
      : set_(bucket_count, Wrapped<hasher>(hash), Wrapped<key_equal>(eq),
             alloc),
        list_(alloc) {}

  linked_hash_set(size_t bucket_count, const hasher& hash,
                  const allocator_type& alloc)
      : linked_hash_set(bucket_count, hash, key_equal(), alloc) {}

  linked_hash_set(size_t bucket_count, const allocator_type& alloc)
      : linked_hash_set(bucket_count, hasher(), key_equal(), alloc) {}

  explicit linked_hash_set(const allocator_type& alloc)
      : linked_hash_set(0, hasher(), key_equal(), alloc) {}

  template <class InputIt>
  linked_hash_set(InputIt first, InputIt last, size_t bucket_count = 0,
                  const hasher& hash = hasher(),
                  const key_equal& eq = key_equal(),
                  const allocator_type& alloc = allocator_type())
      : linked_hash_set(bucket_count, hash, eq, alloc) {
    insert(first, last);
  }

  template <class InputIter>
  linked_hash_set(InputIter first, InputIter last, size_t bucket_count,
                  const hasher& hash, const allocator_type& alloc)
      : linked_hash_set(first, last, bucket_count, hash, key_equal(), alloc) {}

  template <class InputIter>
  linked_hash_set(InputIter first, InputIter last, size_t bucket_count,
                  const allocator_type& alloc)
      : linked_hash_set(first, last, bucket_count, hasher(), key_equal(),
                        alloc) {}

  template <class InputIt>
  linked_hash_set(InputIt first, InputIt last, const allocator_type& alloc)
      : linked_hash_set(first, last, /*bucket_count=*/0, hasher(), key_equal(),
                        alloc) {}

  linked_hash_set(std::initializer_list<key_type> init, size_t bucket_count = 0,
                  const hasher& hash = hasher(),
                  const key_equal& eq = key_equal(),
                  const allocator_type& alloc = allocator_type())
      : linked_hash_set(init.begin(), init.end(), bucket_count, hash, eq,
                        alloc) {}

  linked_hash_set(std::initializer_list<key_type> init, size_t bucket_count,
                  const allocator_type& alloc)
      : linked_hash_set(init, bucket_count, hasher(), key_equal(), alloc) {}

  linked_hash_set(std::initializer_list<key_type> init, size_t bucket_count,
                  const hasher& hash, const allocator_type& alloc)
      : linked_hash_set(init, bucket_count, hash, key_equal(), alloc) {}

  linked_hash_set(std::initializer_list<key_type> init,
                  const allocator_type& alloc)
      : linked_hash_set(init, /*bucket_count=*/0, hasher(), key_equal(),
                        alloc) {}

  linked_hash_set(const linked_hash_set& other)
      : linked_hash_set(other.bucket_count(), other.hash_function(),
                        other.key_eq(), other.get_allocator()) {
    CopyFrom(other);
  }

  linked_hash_set(const linked_hash_set& other, const allocator_type& alloc)
      : linked_hash_set(other.bucket_count(), other.hash_function(),
                        other.key_eq(), alloc) {
    CopyFrom(other);
  }

  linked_hash_set(linked_hash_set&& other) noexcept
      : set_(std::move(other.set_)), list_(std::move(other.list_)) {
    // Since the list and set must agree for other to end up "valid",
    // explicitly clear them.
    other.set_.clear();
    other.list_.clear();
  }

  linked_hash_set(linked_hash_set&& other, const allocator_type& alloc)
      : linked_hash_set(0, other.hash_function(), other.key_eq(), alloc) {
    if (get_allocator() == other.get_allocator()) {
      *this = std::move(other);
    } else {
      CopyFrom(std::move(other));
    }
  }

  linked_hash_set& operator=(const linked_hash_set& other) {
    if (this != &other) {
      // Make a new set, with other's hash/eq/alloc.
      set_ = SetType(other.bucket_count(), other.set_.hash_function(),
                     other.set_.key_eq(), other.get_allocator());
      // Copy the list, with other's allocator.
      list_ = ListType(other.get_allocator());
      CopyFrom(other);
    }
    return *this;
  }

  linked_hash_set& operator=(linked_hash_set&& other) noexcept {
    if (this != &other) {
      set_ = std::move(other.set_);
      list_ = std::move(other.list_);
      other.set_.clear();
      other.list_.clear();
    }
    return *this;
  }

  linked_hash_set& operator=(std::initializer_list<key_type> values) {
    clear();
    insert(values.begin(), values.end());
    return *this;
  }

  // Derive size from set_, as list::size might be O(N).
  size_type size() const { return set_.size(); }
  size_type max_size() const noexcept { return ~size_type{}; }
  bool empty() const { return set_.empty(); }

  // Iteration is list-like, in insertion order.
  // These are all forwarded.
  iterator begin() { return list_.begin(); }
  iterator end() { return list_.end(); }
  const_iterator begin() const { return list_.begin(); }
  const_iterator end() const { return list_.end(); }
  const_iterator cbegin() const { return list_.cbegin(); }
  const_iterator cend() const { return list_.cend(); }
  reverse_iterator rbegin() { return list_.rbegin(); }
  reverse_iterator rend() { return list_.rend(); }
  const_reverse_iterator rbegin() const { return list_.rbegin(); }
  const_reverse_iterator rend() const { return list_.rend(); }
  const_reverse_iterator crbegin() const { return list_.crbegin(); }
  const_reverse_iterator crend() const { return list_.crend(); }
  reference front() { return list_.front(); }
  reference back() { return list_.back(); }
  const_reference front() const { return list_.front(); }
  const_reference back() const { return list_.back(); }

  void pop_front() { erase(begin()); }
  void pop_back() { erase(std::prev(end())); }

  ABSL_ATTRIBUTE_REINITIALIZES void clear() {
    set_.clear();
    list_.clear();
  }

  void reserve(size_t n) { set_.reserve(n); }
  size_t bucket_count() const { return set_.bucket_count(); }
  size_t capacity() const { return set_.capacity(); }
  float load_factor() const { return set_.load_factor(); }

  hasher hash_function() const { return set_.hash_function().fn_; }
  key_equal key_eq() const { return set_.key_eq().fn_; }
  allocator_type get_allocator() const { return list_.get_allocator(); }

  template <typename K = key_type>
  size_type erase(const key_arg<K>& key) {
    auto found = set_.find(key);
    if (found == set_.end()) return 0;
    auto list_it = *found;
    // Erase set entry first since it refers to the list element.
    set_.erase(found);
    list_.erase(list_it);
    return 1;
  }

  iterator erase(const_iterator position) {
    auto found = set_.find(position);
    assert(*found == position);
    set_.erase(found);
    return list_.erase(position);
  }

  iterator erase(const_iterator first, const_iterator last) {
    while (first != last) first = erase(first);
    return first;
  }

  template <typename K = key_type>
  iterator find(const key_arg<K>& key) {
    auto found = set_.find(key);
    if (found == set_.end()) return end();
    return *found;
  }

  template <typename K = key_type>
  const_iterator find(const key_arg<K>& key) const {
    auto found = set_.find(key);
    if (found == set_.end()) return end();
    return *found;
  }

  template <typename K = key_type>
  size_t count(const key_arg<K>& key) const {
    return contains(key) ? 1 : 0;
  }
  template <typename K = key_type>
  bool contains(const key_arg<K>& key) const {
    return set_.contains(key);
  }

  template <typename K = key_type>
  std::pair<iterator, iterator> equal_range(const key_arg<K>& key) {
    auto iter = set_.find(key);
    if (iter == set_.end()) return {end(), end()};
    return {*iter, std::next(*iter)};
  }

  template <typename K = key_type>
  std::pair<const_iterator, const_iterator> equal_range(
      const key_arg<K>& key) const {
    auto iter = set_.find(key);
    if (iter == set_.end()) return {end(), end()};
    return {*iter, std::next(*iter)};
  }

  template <typename K = key_type>
  std::pair<iterator, bool> insert(const key_arg<K>& k) {
    return InsertInternal(list_.end(), k);
  }
  template <typename K = key_type, K* = nullptr>
  std::pair<iterator, bool> insert(key_arg<K>&& k) {
    return InsertInternal(list_.end(), std::move(k));
  }

  template <typename K = key_type,
            std::enable_if_t<
                !std::is_convertible_v<const key_arg<K>&, const_iterator> &&
                    !std::is_convertible_v<const key_arg<K>&, iterator>,
                int> = 0>
  iterator insert(const_iterator hint, const key_arg<K>& k) {
    return InsertInternal(hint, k).first;
  }
  template <
      typename K = key_type, K* = nullptr,
      std::enable_if_t<!std::is_convertible_v<key_arg<K>&&, const_iterator> &&
                           !std::is_convertible_v<key_arg<K>&&, iterator>,
                       int> = 0>
  iterator insert(const_iterator hint, key_arg<K>&& k) {
    return InsertInternal(hint, std::move(k)).first;
  }

  void insert(std::initializer_list<key_type> ilist) {
    insert(ilist.begin(), ilist.end());
  }

  template <class InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) insert(*first);
  }

  insert_return_type insert(node_type&& node) {
    if (node.empty()) return {end(), false, node_type()};
    if (auto [set_itr, inserted] = set_.emplace(node.list_.begin()); inserted) {
      list_.splice(list_.end(), node.list_);
      return {*set_itr, true, node_type()};
    } else {
      return {*set_itr, false, std::move(node)};
    }
  }

  iterator insert(const_iterator, node_type&& node) {
    return insert(std::move(node)).first;
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    return EmplaceInternal(list_.end(), std::forward<Args>(args)...);
  }

  template <typename... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args) {
    return EmplaceInternal(hint, std::forward<Args>(args)...).first;
  }

  template <typename H, typename E>
  void merge(linked_hash_set<Key, H, E, Alloc>& src) {
    auto itr = src.list_.begin();
    while (itr != src.list_.end()) {
      if (contains(*itr)) {
        ++itr;
      } else {
        insert(src.extract(itr++));
      }
    }
  }

  template <typename H, typename E>
  void merge(linked_hash_set<Key, H, E, Alloc>&& src) {
    merge(src);
  }

  node_type extract(const_iterator position) {
    set_.erase(position);
    ListType extracted_node_list;
    extracted_node_list.splice(extracted_node_list.end(), list_, position);
    return node_type(std::move(extracted_node_list));
  }

  template <class K = key_type, typename std::enable_if_t<
                                    !std::is_same<K, iterator>::value, int> = 0>
  node_type extract(const key_arg<K>& key) {
    auto node = set_.extract(key);
    if (node.empty()) return node_type();
    ListType extracted_node_list;
    extracted_node_list.splice(extracted_node_list.end(), list_, node.value());
    return node_type(std::move(extracted_node_list));
  }

  void swap(linked_hash_set& other) noexcept {
    using std::swap;
    swap(set_, other.set_);
    swap(list_, other.list_);
  }

  friend bool operator==(const linked_hash_set& a, const linked_hash_set& b) {
    if (a.size() != b.size()) return false;
    const linked_hash_set* outer = &a;
    const linked_hash_set* inner = &b;
    if (outer->capacity() > inner->capacity()) std::swap(outer, inner);
    for (const value_type& elem : *outer)
      if (!inner->contains(elem)) return false;
    return true;
  }

  friend bool operator!=(const linked_hash_set& a, const linked_hash_set& b) {
    return !(a == b);
  }

  void rehash(size_t n) { set_.rehash(n); }

 private:
  template <typename Other>
  void CopyFrom(Other&& other) {
    for (auto& elem : other.list_) {
      set_.insert(list_.insert(list_.end(), std::move(elem)));
    }
    assert(set_.size() == list_.size());
  }

  template <typename... Args>
  std::pair<iterator, bool> EmplaceInternal(const_iterator hint,
                                            Args&&... args) {
    ListType node_donor;
    auto list_iter =
        node_donor.emplace(node_donor.end(), std::forward<Args>(args)...);
    auto ins = set_.insert(list_iter);
    if (!ins.second) return {*ins.first, false};
    list_.splice(hint, node_donor, list_iter);
    return {list_iter, true};
  }

  template <typename U>
  std::pair<iterator, bool> InsertInternal(const_iterator hint,
                                           U&& key) {  // NOLINT(build/c++11)
    bool constructed = false;
    auto set_iter = set_.lazy_emplace(key, [&](const auto& ctor) {
      constructed = true;
      ctor(list_.emplace(hint, std::forward<U>(key)));
    });
    return {*set_iter, constructed};
  }

  // The set component, used for speedy lookups.
  SetType set_;

  // The list component, used for maintaining insertion order.
  ListType list_;
};

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_CONTAINER_LINKED_HASH_SET_H_
