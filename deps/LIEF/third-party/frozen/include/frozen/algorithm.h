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

#ifndef FROZEN_LETITGO_ALGORITHM_H
#define FROZEN_LETITGO_ALGORITHM_H

#include "frozen/bits/basic_types.h"
#include "frozen/bits/version.h"
#include "frozen/string.h"

namespace frozen {

// 'search' implementation if C++17 is not available
// https://en.cppreference.com/w/cpp/algorithm/search
template<class ForwardIterator, class Searcher>
ForwardIterator search(ForwardIterator first, ForwardIterator last, const Searcher & searcher)
{
  return searcher(first, last).first;
}

// text book implementation from
// https://en.wikipedia.org/wiki/Knuth%E2%80%93Morris%E2%80%93Pratt_algorithm

template <std::size_t size> class knuth_morris_pratt_searcher {
  bits::carray<std::ptrdiff_t, size> step_;
  bits::carray<char, size> needle_;

  static constexpr bits::carray<std::ptrdiff_t, size>
  build_kmp_cache(char const (&needle)[size + 1]) {
    std::ptrdiff_t cnd = 0;
    bits::carray<std::ptrdiff_t, size> cache(-1);
    for (std::size_t pos = 1; pos < size; ++pos) {
      if (needle[pos] == needle[cnd]) {
        cache[pos] = cache[cnd];
        cnd += 1;
      } else {
        cache[pos] = cnd;
        cnd = cache[cnd];
        while (cnd >= 0 && needle[pos] != needle[cnd])
          cnd = cache[cnd];
        cnd += 1;
      }
    }
    return cache;
  }

public:
  constexpr knuth_morris_pratt_searcher(char const (&needle)[size + 1])
    : step_{build_kmp_cache(needle)}, needle_(needle) {}

  template <class ForwardIterator>
  constexpr std::pair<ForwardIterator, ForwardIterator> operator()(ForwardIterator first, ForwardIterator last) const {
    std::size_t i = 0;
    ForwardIterator iter = first;
    while (iter != last) {
      if (needle_[i] == *iter) {
        if (i == (size - 1))
          return { iter - i, iter - i + size };
        ++i;
        ++iter;
      } else {
        if (step_[i] > -1) {
          i = step_[i];
        } else {
          ++iter;
          i = 0;
        }
      }
    }
    return { last, last };
  }
};

template <std::size_t N>
constexpr knuth_morris_pratt_searcher<N - 1> make_knuth_morris_pratt_searcher(char const (&needle)[N]) {
  return {needle};
}

// text book implementation from
// https://en.wikipedia.org/wiki/Boyer%E2%80%93Moore%E2%80%93Horspool_algorithm

template <std::size_t size> class boyer_moore_searcher {
  using skip_table_type = bits::carray<std::ptrdiff_t, sizeof(char) << 8>;
  using suffix_table_type = bits::carray<std::ptrdiff_t, size>;

  skip_table_type skip_table_;
  suffix_table_type suffix_table_;
  bits::carray<char, size> needle_;

  constexpr auto build_skip_table(char const (&needle)[size + 1]) {
    skip_table_type skip_table(size);
    for (std::size_t i = 0; i < size - 1; ++i)
      skip_table[needle[i]] -= i + 1;
    return skip_table;
  }

  constexpr bool is_prefix(char const (&needle)[size + 1], std::size_t pos) {
    std::size_t suffixlen = size - pos;
    
    for (std::size_t i = 0; i < suffixlen; i++) {
      if (needle[i] != needle[pos + i])
        return false;
    }
    return true;
  }

  constexpr std::size_t suffix_length(char const (&needle)[size + 1],
                                      std::size_t pos) {
    // increment suffix length slen to the first mismatch or beginning
    // of the word
    for (std::size_t slen = 0; slen < pos ; slen++)
      if (needle[pos - slen] != needle[size - 1 - slen])
        return slen;

    return pos;
  }

  constexpr auto build_suffix_table(char const (&needle)[size + 1]) {
    suffix_table_type suffix;
    std::ptrdiff_t last_prefix_index = size - 1;

    // first loop
    for (std::ptrdiff_t p = size - 1; p >= 0; p--) {
      if (is_prefix(needle, p + 1))
        last_prefix_index = p + 1;

      suffix[p] = last_prefix_index + (size - 1 - p);
    }

    // second loop
    for (std::size_t p = 0; p < size - 1; p++) {
      auto slen = suffix_length(needle, p);
      if (needle[p - slen] != needle[size - 1 - slen])
        suffix[size - 1 - slen] = size - 1 - p + slen;

    }
    return suffix;
  }

public:
  constexpr boyer_moore_searcher(char const (&needle)[size + 1])
    : skip_table_{build_skip_table(needle)},
      suffix_table_{build_suffix_table(needle)},
      needle_(needle) {}

  template <class RandomAccessIterator>
  constexpr std::pair<RandomAccessIterator, RandomAccessIterator> operator()(RandomAccessIterator first, RandomAccessIterator last) const {
    if (size == 0)
      return { first, first };

    if (size > size_t(last - first))
      return { last, last };

    RandomAccessIterator iter = first + size - 1;
    while (true) {
      std::ptrdiff_t j = size - 1;
      while (j > 0 && (*iter == needle_[j])) {
        --iter;
        --j;
      }
      if (j == 0 && *iter == needle_[0])
        return { iter, iter + size};

      std::ptrdiff_t jump = std::max(skip_table_[*iter], suffix_table_[j]);
      if (jump >= last - iter)
        return { last, last };
      iter += jump;
    }
  }
};

template <std::size_t N>
constexpr boyer_moore_searcher<N - 1> make_boyer_moore_searcher(char const (&needle)[N]) {
  return {needle};
}

} // namespace frozen

#endif
