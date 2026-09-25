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

// inspired from http://stevehanov.ca/blog/index.php?id=119
#ifndef FROZEN_LETITGO_PMH_H
#define FROZEN_LETITGO_PMH_H

#include "frozen/bits/algorithms.h"
#include "frozen/bits/basic_types.h"

#include <array>
#include <limits>

namespace frozen {

namespace bits {

// Function object for sorting buckets in decreasing order of size
struct bucket_size_compare {
  template <typename B>
  bool constexpr operator()(B const &b0,
                            B const &b1) const {
    return b0.size() > b1.size();
  }
};

// Step One in pmh routine is to take all items and hash them into buckets,
// with some collisions. Then process those buckets further to build a perfect
// hash function.
// pmh_buckets represents the initial placement into buckets.

template <size_t M>
struct pmh_buckets {
  // Step 0: Bucket max is 2 * sqrt M
  // TODO: Come up with justification for this, should it not be O(log M)?
  static constexpr auto bucket_max = 2 * (1u << (log(M) / 2));

  using bucket_t = cvector<std::size_t, bucket_max>;
  carray<bucket_t, M> buckets;
  uint64_t seed;

  // Represents a reference to a bucket. This is used because the buckets
  // have to be sorted, but buckets are big, making it slower than sorting refs
  struct bucket_ref {
    unsigned hash;
    const bucket_t * ptr;

    // Forward some interface of bucket
    using value_type = typename bucket_t::value_type;
    using const_iterator = typename bucket_t::const_iterator;

    constexpr auto size() const { return ptr->size(); }
    constexpr const auto & operator[](std::size_t idx) const { return (*ptr)[idx]; }
    constexpr auto begin() const { return ptr->begin(); }
    constexpr auto end() const { return ptr->end(); }
  };

  // Make a bucket_ref for each bucket
  template <std::size_t... Is>
  carray<bucket_ref, M> constexpr make_bucket_refs(std::index_sequence<Is...>) const {
    return {{ bucket_ref{Is, &buckets[Is]}... }};
  }

  // Makes a bucket_ref for each bucket and sorts them by size
  carray<bucket_ref, M> constexpr get_sorted_buckets() const {
    carray<bucket_ref, M> result{this->make_bucket_refs(std::make_index_sequence<M>())};
    bits::quicksort(result.begin(), result.end() - 1, bucket_size_compare{});
    return result;
  }
};

template <size_t M, class Item, size_t N, class Hash, class Key, class PRG>
pmh_buckets<M> constexpr make_pmh_buckets(const carray<Item, N> & items,
                                Hash const & hash,
                                Key const & key,
                                PRG & prg) {
  using result_t = pmh_buckets<M>;
  result_t result{};
  bool rejected = false;
  // Continue until all items are placed without exceeding bucket_max
  while (1) {
    for (auto & b : result.buckets) {
      b.clear();
    }
    result.seed = prg();
    rejected = false;
    for (std::size_t i = 0; i < N; ++i) {
      auto & bucket = result.buckets[hash(key(items[i]), static_cast<size_t>(result.seed)) % M];
      if (bucket.size() >= result_t::bucket_max) {
        rejected = true;
        break;
      }
      bucket.push_back(i);
    }
    if (!rejected) { return result; }
  }
}

// Check if an item appears in a cvector
template<class T, size_t N>
constexpr bool all_different_from(cvector<T, N> & data, T & a) {
  for (std::size_t i = 0; i < data.size(); ++i)
    if (data[i] == a)
      return false;

  return true;
}

// Represents either an index to a data item array, or a seed to be used with
// a hasher. Seed must have high bit of 1, value has high bit of zero.
struct seed_or_index {
  using value_type = uint64_t;

private:
  static constexpr value_type MINUS_ONE = std::numeric_limits<value_type>::max();
  static constexpr value_type HIGH_BIT = ~(MINUS_ONE >> 1);

  value_type value_ = 0;

public:
  constexpr value_type value() const { return value_; }
  constexpr bool is_seed() const { return value_ & HIGH_BIT; }

  constexpr seed_or_index(bool is_seed, value_type value)
    : value_(is_seed ? (value | HIGH_BIT) : (value & ~HIGH_BIT)) {}

  constexpr seed_or_index() = default;
  constexpr seed_or_index(const seed_or_index &) = default;
  constexpr seed_or_index & operator =(const seed_or_index &) = default;
};

// Represents the perfect hash function created by pmh algorithm
template <std::size_t M, class Hasher>
struct pmh_tables {
  uint64_t first_seed_;
  carray<seed_or_index, M> first_table_;
  carray<std::size_t, M> second_table_;
  Hasher hash_;

  template <typename KeyType>
  constexpr std::size_t lookup(const KeyType & key) const {
    return lookup(key, hash_);
  }

  // Looks up a given key, to find its expected index in carray<Item, N>
  // Always returns a valid index, must use KeyEqual test after to confirm.
  template <typename KeyType, typename HasherType>
  constexpr std::size_t lookup(const KeyType & key, const HasherType& hasher) const {
    auto const d = first_table_[hasher(key, static_cast<size_t>(first_seed_)) % M];
    if (!d.is_seed()) { return static_cast<std::size_t>(d.value()); } // this is narrowing uint64 -> size_t but should be fine
    else { return second_table_[hasher(key, static_cast<std::size_t>(d.value())) % M]; }
  }
};

// Make pmh tables for given items, hash function, prg, etc.
template <std::size_t M, class Item, std::size_t N, class Hash, class Key, class PRG>
pmh_tables<M, Hash> constexpr make_pmh_tables(const carray<Item, N> &
                                                               items,
                                                           Hash const &hash,
                                                           Key const &key,
                                                           PRG prg) {
  // Step 1: Place all of the keys into buckets
  auto step_one = make_pmh_buckets<M>(items, hash, key, prg);

  // Step 2: Sort the buckets to process the ones with the most items first.
  auto buckets = step_one.get_sorted_buckets();

  // G becomes the first hash table in the resulting pmh function
  carray<seed_or_index, M> G; // Default constructed to "index 0"

  // H becomes the second hash table in the resulting pmh function
  constexpr std::size_t UNUSED = std::numeric_limits<std::size_t>::max();
  carray<std::size_t, M> H;
  H.fill(UNUSED);

  // Step 3: Map the items in buckets into hash tables.
  for (const auto & bucket : buckets) {
    auto const bsize = bucket.size();

    if (bsize == 1) {
      // Store index to the (single) item in G
      // assert(bucket.hash == hash(key(items[bucket[0]]), step_one.seed) % M);
      G[bucket.hash] = {false, static_cast<uint64_t>(bucket[0])};
    } else if (bsize > 1) {

      // Repeatedly try different H of d until we find a hash function
      // that places all items in the bucket into free slots
      seed_or_index d{true, prg()};
      cvector<std::size_t, decltype(step_one)::bucket_max> bucket_slots;

      while (bucket_slots.size() < bsize) {
        auto slot = hash(key(items[bucket[bucket_slots.size()]]), static_cast<size_t>(d.value())) % M;

        if (H[slot] != UNUSED || !all_different_from(bucket_slots, slot)) {
          bucket_slots.clear();
          d = {true, prg()};
          continue;
        }

        bucket_slots.push_back(slot);
      }

      // Put successful seed in G, and put indices to items in their slots
      // assert(bucket.hash == hash(key(items[bucket[0]]), step_one.seed) % M);
      G[bucket.hash] = d;
      for (std::size_t i = 0; i < bsize; ++i)
        H[bucket_slots[i]] = bucket[i];
    }
  }

  // Any unused entries in the H table have to get changed to zero.
  // This is because hashing should not fail or return an out-of-bounds entry.
  // A lookup fails after we apply user-supplied KeyEqual to the query and the
  // key found by hashing. Sending such queries to zero cannot hurt.
  for (std::size_t i = 0; i < M; ++i)
    if (H[i] == UNUSED)
      H[i] = 0;

  return {step_one.seed, G, H, hash};
}

} // namespace bits

} // namespace frozen

#endif
