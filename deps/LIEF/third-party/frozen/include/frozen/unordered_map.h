/*
 * Frozen
 * Copyright 2016 QuarksLab
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef FROZEN_LETITGO_UNORDERED_MAP_H
#define FROZEN_LETITGO_UNORDERED_MAP_H

#include "frozen/bits/basic_types.h"
#include "frozen/bits/constexpr_assert.h"
#include "frozen/bits/elsa.h"
#include "frozen/bits/exceptions.h"
#include "frozen/bits/pmh.h"
#include "frozen/bits/version.h"
#include "frozen/random.h"

#include <tuple>
#include <functional>

namespace frozen {

namespace bits {

struct GetKey {
  template <class KV> constexpr auto const &operator()(KV const &kv) const {
    return kv.first;
  }
};

} // namespace bits

template <class Key, class Value, std::size_t N, typename Hash = anna<Key>,
          class KeyEqual = std::equal_to<Key>>
class unordered_map {
  static constexpr std::size_t storage_size =
      bits::next_highest_power_of_two(N) * (N < 32 ? 2 : 1); // size adjustment to prevent high collision rate for small sets
  using container_type = bits::carray<std::pair<Key, Value>, N>;
  using tables_type = bits::pmh_tables<storage_size, Hash>;

  KeyEqual const equal_;
  container_type items_;
  tables_type tables_;

public:
  /* typedefs */
  using Self = unordered_map<Key, Value, N, Hash, KeyEqual>;
  using key_type = Key;
  using mapped_type = Value;
  using value_type = typename container_type::value_type;
  using size_type = typename container_type::size_type;
  using difference_type = typename container_type::difference_type;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using reference = typename container_type::reference;
  using const_reference = typename container_type::const_reference;
  using pointer = typename container_type::pointer;
  using const_pointer = typename container_type::const_pointer;
  using iterator = typename container_type::iterator;
  using const_iterator = typename container_type::const_iterator;

public:
  /* constructors */
  unordered_map(unordered_map const &) = default;
  constexpr unordered_map(container_type items,
                          Hash const &hash, KeyEqual const &equal)
      : equal_{equal}
      , items_{items}
      , tables_{
            bits::make_pmh_tables<storage_size>(
                items_, hash, bits::GetKey{}, default_prg_t{})} {}
  explicit constexpr unordered_map(container_type items)
      : unordered_map{items, Hash{}, KeyEqual{}} {}

  constexpr unordered_map(std::initializer_list<value_type> items,
                          Hash const & hash, KeyEqual const & equal)
      : unordered_map{container_type{items}, hash, equal} {
        constexpr_assert(items.size() == N, "Inconsistent initializer_list size and type size argument");
      }

  constexpr unordered_map(std::initializer_list<value_type> items)
      : unordered_map{items, Hash{}, KeyEqual{}} {}

  /* iterators */
  constexpr iterator begin() { return items_.begin(); }
  constexpr iterator end() { return items_.end(); }
  constexpr const_iterator begin() const { return items_.begin(); }
  constexpr const_iterator end() const { return items_.end(); }
  constexpr const_iterator cbegin() const { return items_.cbegin(); }
  constexpr const_iterator cend() const { return items_.cend(); }

  /* capacity */
  constexpr bool empty() const { return !N; }
  constexpr size_type size() const { return N; }
  constexpr size_type max_size() const { return N; }

  /* lookup */
  template <class KeyType, class Hasher, class Equal>
  constexpr std::size_t count(KeyType const &key, Hasher const &hash, Equal const &equal) const {
    auto const &kv = lookup(key, hash);
    return equal(kv.first, key);
  }
  template <class KeyType>
  constexpr std::size_t count(KeyType const &key) const {
    return count(key, hash_function(), key_eq());
  }

  template <class KeyType, class Hasher, class Equal>
  constexpr Value const &at(KeyType const &key, Hasher const &hash, Equal const &equal) const {
    return at_impl(*this, key, hash, equal);
  }
  template <class KeyType, class Hasher, class Equal>
  constexpr Value &at(KeyType const &key, Hasher const &hash, Equal const &equal) {
    return at_impl(*this, key, hash, equal);
  }
  template <class KeyType>
  constexpr Value const &at(KeyType const &key) const {
    return at(key, hash_function(), key_eq());
  }
  template <class KeyType>
  constexpr Value &at(KeyType const &key) {
    return at(key, hash_function(), key_eq());
  }

  template <class KeyType, class Hasher, class Equal>
  constexpr const_iterator find(KeyType const &key, Hasher const &hash, Equal const &equal) const {
    return find_impl(*this, key, hash, equal);
  }
  template <class KeyType, class Hasher, class Equal>
  constexpr iterator find(KeyType const &key, Hasher const &hash, Equal const &equal) {
    return find_impl(*this, key, hash, equal);
  }
  template <class KeyType>
  constexpr const_iterator find(KeyType const &key) const {
    return find(key, hash_function(), key_eq());
  }
  template <class KeyType>
  constexpr iterator find(KeyType const &key) {
    return find(key, hash_function(), key_eq());
  }
  
  template <class KeyType>
  constexpr bool contains(KeyType const &key) const {
    return this->find(key) != this->end();
  }

  template <class KeyType, class Hasher, class Equal>
  constexpr std::pair<const_iterator, const_iterator> equal_range(KeyType const &key, Hasher const &hash, Equal const &equal) const {
    return equal_range_impl(*this, key, hash, equal);
  }
  template <class KeyType, class Hasher, class Equal>
  constexpr std::pair<iterator, iterator> equal_range(KeyType const &key, Hasher const &hash, Equal const &equal) {
    return equal_range_impl(*this, key, hash, equal);
  }
  template <class KeyType>
  constexpr std::pair<const_iterator, const_iterator> equal_range(KeyType const &key) const {
    return equal_range(key, hash_function(), key_eq());
  }
  template <class KeyType>
  constexpr std::pair<iterator, iterator> equal_range(KeyType const &key) {
    return equal_range(key, hash_function(), key_eq());
  }

  /* bucket interface */
  constexpr std::size_t bucket_count() const { return storage_size; }
  constexpr std::size_t max_bucket_count() const { return storage_size; }

  /* observers*/
  constexpr const hasher& hash_function() const { return tables_.hash_; }
  constexpr const key_equal& key_eq() const { return equal_; }

private:
  template <class This, class KeyType, class Hasher, class Equal>
  static inline constexpr auto& at_impl(This&& self, KeyType const &key, Hasher const &hash, Equal const &equal) {
    auto& kv = self.lookup(key, hash);
    if (equal(kv.first, key))
      return kv.second;
    else
      FROZEN_THROW_OR_ABORT(std::out_of_range("unknown key"));
  }

  template <class This, class KeyType, class Hasher, class Equal>
  static inline constexpr auto find_impl(This&& self, KeyType const &key, Hasher const &hash, Equal const &equal) {
    auto& kv = self.lookup(key, hash);
    if (equal(kv.first, key))
      return &kv;
    else
      return self.items_.end();
  }

  template <class This, class KeyType, class Hasher, class Equal>
  static inline constexpr auto equal_range_impl(This&& self, KeyType const &key, Hasher const &hash, Equal const &equal) {
    auto& kv = self.lookup(key, hash);
    using kv_ptr = decltype(&kv);
    if (equal(kv.first, key))
      return std::pair<kv_ptr, kv_ptr>{&kv, &kv + 1};
    else
      return std::pair<kv_ptr, kv_ptr>{self.items_.end(), self.items_.end()};
  }

  template <class This, class KeyType, class Hasher>
  static inline constexpr auto& lookup_impl(This&& self, KeyType const &key, Hasher const &hash) {
    return self.items_[self.tables_.lookup(key, hash)];
  }

  template <class KeyType, class Hasher>
  constexpr auto const& lookup(KeyType const &key, Hasher const &hash) const {
    return lookup_impl(*this, key, hash);
  }
  template <class KeyType, class Hasher>
  constexpr auto& lookup(KeyType const &key, Hasher const &hash) {
    return lookup_impl(*this, key, hash);
  }
};

template <typename T, typename U, std::size_t N>
constexpr auto make_unordered_map(std::pair<T, U> const (&items)[N]) {
  return unordered_map<T, U, N>{items};
}

template <typename T, typename U, std::size_t N, typename Hasher, typename Equal>
constexpr auto make_unordered_map(
        std::pair<T, U> const (&items)[N],
        Hasher const &hash = elsa<T>{},
        Equal const &equal = std::equal_to<T>{}) {
  return unordered_map<T, U, N, Hasher, Equal>{items, hash, equal};
}

template <typename T, typename U, std::size_t N>
constexpr auto make_unordered_map(std::array<std::pair<T, U>, N> const &items) {
  return unordered_map<T, U, N>{items};
}

template <typename T, typename U, std::size_t N, typename Hasher, typename Equal>
constexpr auto make_unordered_map(
        std::array<std::pair<T, U>, N> const &items,
        Hasher const &hash = elsa<T>{},
        Equal const &equal = std::equal_to<T>{}) {
  return unordered_map<T, U, N, Hasher, Equal>{items, hash, equal};
}

} // namespace frozen

#endif
