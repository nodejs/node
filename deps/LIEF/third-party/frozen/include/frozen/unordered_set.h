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

#ifndef FROZEN_LETITGO_UNORDERED_SET_H
#define FROZEN_LETITGO_UNORDERED_SET_H

#include "frozen/bits/basic_types.h"
#include "frozen/bits/elsa.h"
#include "frozen/bits/pmh.h"
#include "frozen/bits/version.h"
#include "frozen/random.h"

#include <utility>

namespace frozen {

namespace bits {

struct Get {
  template <class T> constexpr T const &operator()(T const &key) const {
    return key;
  }
};

} // namespace bits

template <class Key, std::size_t N, typename Hash = elsa<Key>,
          class KeyEqual = std::equal_to<Key>>
class unordered_set : private KeyEqual {
  static constexpr std::size_t storage_size =
      bits::next_highest_power_of_two(N) * (N < 32 ? 2 : 1); // size adjustment to prevent high collision rate for small sets
  using container_type = bits::carray<Key, N>;
  using tables_type = bits::pmh_tables<storage_size, Hash>;

  container_type keys_;
  tables_type tables_;

public:
  /* typedefs */
  using key_type = Key;
  using value_type = Key;
  using size_type = typename container_type::size_type;
  using difference_type = typename container_type::difference_type;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using const_reference = typename container_type::const_reference;
  using reference = const_reference;
  using const_pointer = typename container_type::const_pointer;
  using pointer = const_pointer;
  using const_iterator = typename container_type::const_iterator;
  using iterator = const_iterator;

public:
  /* constructors */
  unordered_set(unordered_set const &) = default;
  constexpr unordered_set(container_type keys, Hash const &hash,
                          KeyEqual const &equal)
      : KeyEqual{equal}
      , keys_{keys}
      , tables_{bits::make_pmh_tables<storage_size>(
            keys_, hash, equal, bits::Get{}, default_prg_t{})} {}
  explicit constexpr unordered_set(container_type keys)
      : unordered_set{keys, Hash{}, KeyEqual{}} {}

  constexpr unordered_set(std::initializer_list<Key> keys)
      : unordered_set{keys, Hash{}, KeyEqual{}} {}

  constexpr unordered_set(std::initializer_list<Key> keys, Hash const & hash, KeyEqual const & equal)
      : unordered_set{container_type{keys}, hash, equal} {
      }

  /* iterators */
  constexpr const_iterator begin() const { return keys_.begin(); }
  constexpr const_iterator end() const { return keys_.end(); }
  constexpr const_iterator cbegin() const { return keys_.begin(); }
  constexpr const_iterator cend() const { return keys_.end(); }

  /* capacity */
  constexpr bool empty() const { return !N; }
  constexpr size_type size() const { return N; }
  constexpr size_type max_size() const { return N; }

  /* lookup */
  template <class KeyType>
  constexpr std::size_t count(KeyType const &key) const {
    return find(key, hash_function(), key_eq()) != end();
  }

  template <class KeyType, class Hasher, class Equal>
  constexpr const_iterator find(KeyType const &key, Hasher const &hash, Equal const &equal) const {
    auto const pos = tables_.lookup(key, hash);
    auto it = keys_.begin() + pos;
    if (it != keys_.end() && equal(*it, key))
      return it;
    else
      return keys_.end();
  }
  template <class KeyType>
  constexpr const_iterator find(KeyType const &key) const {
    auto const pos = tables_.lookup(key, hash_function());
    auto it = keys_.begin() + pos;
    if (it != keys_.end() && key_eq()(*it, key))
      return it;
    else
      return keys_.end();
  }

  template <class KeyType>
  constexpr bool contains(KeyType const &key) const {
    return this->find(key) != keys_.end();
  }

  template <class KeyType>
  constexpr std::pair<const_iterator, const_iterator> equal_range(KeyType const &key) const {
    auto const it = find(key);
    if (it != end())
      return {it, it + 1};
    else
      return {keys_.end(), keys_.end()};
  }

  /* bucket interface */
  constexpr std::size_t bucket_count() const { return storage_size; }
  constexpr std::size_t max_bucket_count() const { return storage_size; }

  /* observers*/
  constexpr const hasher& hash_function() const { return tables_.hash_function(); }
  constexpr const key_equal& key_eq() const { return static_cast<KeyEqual const&>(*this); }
};

template <typename T, std::size_t N>
constexpr auto make_unordered_set(T const (&keys)[N]) {
  return unordered_set<T, N>{keys};
}

template <typename T, std::size_t N, typename Hasher, typename Equal>
constexpr auto make_unordered_set(T const (&keys)[N], Hasher const& hash, Equal const& equal) {
  return unordered_set<T, N, Hasher, Equal>{keys, hash, equal};
}

template <typename T, std::size_t N>
constexpr auto make_unordered_set(std::array<T, N> const &keys) {
  return unordered_set<T, N>{keys};
}

template <typename T, std::size_t N, typename Hasher, typename Equal>
constexpr auto make_unordered_set(std::array<T, N> const &keys, Hasher const& hash, Equal const& equal) {
  return unordered_set<T, N, Hasher, Equal>{keys, hash, equal};
}

#ifdef FROZEN_LETITGO_HAS_DEDUCTION_GUIDES

template <class T, class... Args>
unordered_set(T, Args...) -> unordered_set<T, sizeof...(Args) + 1>;

#endif

} // namespace frozen

#endif
