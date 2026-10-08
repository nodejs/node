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

#ifndef FROZEN_LETITGO_BITS_ALGORITHMS_H
#define FROZEN_LETITGO_BITS_ALGORITHMS_H

#include "frozen/bits/basic_types.h"

#include <limits>
#include <tuple>

namespace frozen {

namespace bits {

auto constexpr next_highest_power_of_two(std::size_t v) {
  // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
  constexpr auto trip_count = std::numeric_limits<decltype(v)>::digits;
  v--;
  for(std::size_t i = 1; i < trip_count; i <<= 1)
    v |= v >> i;
  v++;
  return v;
}

template<class T>
auto constexpr log(T v) {
  std::size_t n = 0;
  while (v > 1) {
    n += 1;
    v >>= 1;
  }
  return n;
}

constexpr std::size_t bit_weight(std::size_t n) {
  return (n <= 8*sizeof(unsigned int))
    + (n <= 8*sizeof(unsigned long))
    + (n <= 8*sizeof(unsigned long long))
    + (n <= 128);
}

unsigned int select_uint_least(std::integral_constant<std::size_t, 4>);
unsigned long select_uint_least(std::integral_constant<std::size_t, 3>);
unsigned long long select_uint_least(std::integral_constant<std::size_t, 2>);
template<std::size_t N>
unsigned long long select_uint_least(std::integral_constant<std::size_t, N>) {
  static_assert(N < 2, "unsupported type size");
  return {};
}


template<std::size_t N>
using select_uint_least_t = decltype(select_uint_least(std::integral_constant<std::size_t, bit_weight(N)>()));

template <typename Iter, typename Compare>
constexpr auto min_element(Iter begin, const Iter end,
                           Compare const &compare) {
  auto result = begin;
  while (begin != end) {
    if (compare(*begin, *result)) {
      result = begin;
    }
    ++begin;
  }
  return result;
}

template <class T>
constexpr void cswap(T &a, T &b) {
  auto tmp = a;
  a = b;
  b = tmp;
}

template <class T, class U>
constexpr void cswap(std::pair<T, U> & a, std::pair<T, U> & b) {
  cswap(a.first, b.first);
  cswap(a.second, b.second);
}

template <class... Tys, std::size_t... Is>
constexpr void cswap(std::tuple<Tys...> &a, std::tuple<Tys...> &b, std::index_sequence<Is...>) {
  using swallow = int[];
  (void) swallow{(cswap(std::get<Is>(a), std::get<Is>(b)), 0)...};
}

template <class... Tys>
constexpr void cswap(std::tuple<Tys...> &a, std::tuple<Tys...> &b) {
  cswap(a, b, std::make_index_sequence<sizeof...(Tys)>());
}

template <typename Iter>
constexpr void iter_swap(Iter a, Iter b) {
  cswap(*a, *b);
}

template <typename Iterator, class Compare>
constexpr Iterator partition(Iterator left, Iterator right, Compare const &compare) {
  auto pivot = left + (right - left) / 2;
  iter_swap(right, pivot);
  pivot = right;
  for (auto it = left; 0 < right - it; ++it) {
    if (compare(*it, *pivot)) {
      iter_swap(it, left);
      left++;
    }
  }
  iter_swap(pivot, left);
  pivot = left;
  return pivot;
}

template <typename Iterator, class Compare>
constexpr void quicksort(Iterator left, Iterator right, Compare const &compare) {
  while (0 < right - left) {
    auto new_pivot = bits::partition(left, right, compare);
    quicksort(left, new_pivot, compare);
    left = new_pivot + 1;
  }
}

template <typename Container, class Compare>
constexpr Container quicksort(Container const &array,
                                     Compare const &compare) {
  Container res = array;
  quicksort(res.begin(), res.end() - 1, compare);
  return res;
}

template <class T, class Compare> struct LowerBound {
  T const &value_;
  Compare const &compare_;
  constexpr LowerBound(T const &value, Compare const &compare)
      : value_(value), compare_(compare) {}

  template <class ForwardIt>
  inline constexpr ForwardIt doit_fast(ForwardIt first,
                                  std::integral_constant<std::size_t, 0>) {
    return first;
  }

  template <class ForwardIt, std::size_t N>
  inline constexpr ForwardIt doit_fast(ForwardIt first,
                                  std::integral_constant<std::size_t, N>) {
    auto constexpr step = N / 2;
    static_assert(N/2 == N - N / 2 - 1, "power of two minus 1");
    auto it = first + step;
    auto next_it = compare_(*it, value_) ? it + 1 : first;
    return doit_fast(next_it, std::integral_constant<std::size_t, N / 2>{});
  }

  template <class ForwardIt, std::size_t N>
  inline constexpr ForwardIt doitfirst(ForwardIt first, std::integral_constant<std::size_t, N>, std::integral_constant<bool, true>) {
    return doit_fast(first, std::integral_constant<std::size_t, N>{});
  }

  template <class ForwardIt, std::size_t N>
  inline constexpr ForwardIt doitfirst(ForwardIt first, std::integral_constant<std::size_t, N>, std::integral_constant<bool, false>) {
    auto constexpr next_power = next_highest_power_of_two(N);
    auto constexpr next_start = next_power / 2 - 1;
    auto it = first + next_start;
    if (compare_(*it, value_)) {
      auto constexpr next = N - next_start - 1;
      return doitfirst(it + 1, std::integral_constant<std::size_t, next>{}, std::integral_constant<bool, next_highest_power_of_two(next) - 1 == next>{});
    }
    else
      return doit_fast(first, std::integral_constant<std::size_t, next_start>{});
  }

  template <class ForwardIt>
  inline constexpr ForwardIt doitfirst(ForwardIt first, std::integral_constant<std::size_t, 1>, std::integral_constant<bool, false>) {
    return doit_fast(first, std::integral_constant<std::size_t, 1>{});
  }
};

template <std::size_t N, class ForwardIt, class T, class Compare>
constexpr ForwardIt lower_bound(ForwardIt first, const T &value, Compare const &compare) {
  return LowerBound<T, Compare>{value, compare}.doitfirst(first, std::integral_constant<std::size_t, N>{}, std::integral_constant<bool, next_highest_power_of_two(N) - 1 == N>{});
}

template <std::size_t N, class Compare, class ForwardIt, class T>
constexpr bool binary_search(ForwardIt first, const T &value,
                             Compare const &compare) {
  ForwardIt where = lower_bound<N>(first, value, compare);
  return (!(where == first + N) && !(compare(value, *where)));
}


template<class InputIt1, class InputIt2>
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2)
{
  for (; first1 != last1; ++first1, ++first2) {
    if (!(*first1 == *first2)) {
      return false;
    }
  }
  return true;
}

template<class InputIt1, class InputIt2>
constexpr bool lexicographical_compare(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2)
{
  for (; (first1 != last1) && (first2 != last2); ++first1, ++first2) {
    if (*first1 < *first2)
      return true;
    if (*first2 < *first1)
      return false;
  }
  return (first1 == last1) && (first2 != last2);
}

} // namespace bits
} // namespace frozen

#endif
