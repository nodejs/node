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

#ifndef FROZEN_LETITGO_BASIC_TYPES_H
#define FROZEN_LETITGO_BASIC_TYPES_H

#include "frozen/bits/exceptions.h"

#include <array>
#include <utility>
#include <iterator>
#include <string>

namespace frozen {

namespace bits {

// used as a fake argument for frozen::make_set and frozen::make_map in the case of N=0
struct ignored_arg {};

template <class T, std::size_t N>
class cvector {
  T data [N] = {}; // zero-initialization for scalar type T, default-initialized otherwise
  std::size_t dsize = 0;

public:
  // Container typdefs
  using value_type = T;
  using reference = value_type &;
  using const_reference = const value_type &;
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  // Constructors
  constexpr cvector(void) = default;
  constexpr cvector(size_type count, const T& value) : dsize(count) {
    for (std::size_t i = 0; i < N; ++i)
      data[i] = value;
  }

  // Iterators
  constexpr iterator begin() noexcept { return data; }
  constexpr iterator end() noexcept { return data + dsize; }

  // Capacity
  constexpr size_type size() const { return dsize; }

  // Element access
  constexpr       reference operator[](std::size_t index) { return data[index]; }
  constexpr const_reference operator[](std::size_t index) const { return data[index]; }

  constexpr       reference back() { return data[dsize - 1]; }
  constexpr const_reference back() const { return data[dsize - 1]; }

  // Modifiers
  constexpr void push_back(const T & a) { data[dsize++] = a; }
  constexpr void push_back(T && a) { data[dsize++] = std::move(a); }
  constexpr void pop_back() { --dsize; }

  constexpr void clear() { dsize = 0; }
};

template <class T, std::size_t N>
class carray {
  T data_ [N] = {}; // zero-initialization for scalar type T, default-initialized otherwise

  template <std::size_t M, std::size_t... I>
  constexpr carray(T const (&init)[M], std::index_sequence<I...>)
      : data_{init[I]...} {}
  template <class Iter, std::size_t... I>
  constexpr carray(Iter iter, std::index_sequence<I...>)
      : data_{((void)I, *iter++)...} {}

public:
  // Container typdefs
  using value_type = T;
  using reference = value_type &;
  using const_reference = const value_type &;
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  // Constructors
  constexpr carray(void) = default;
  template <std::size_t M>
  constexpr carray(T const (&init)[M])
    : carray(init, std::make_index_sequence<N>())
  {
    static_assert(M >= N, "Cannot initialize a carray with an smaller array");
  }
  template <std::size_t M>
  constexpr carray(std::array<T, M> const &init)
    : carray(&init[0], std::make_index_sequence<N>())
  {
    static_assert(M >= N, "Cannot initialize a carray with an smaller array");
  }
  constexpr carray(std::initializer_list<T> init)
    : carray(init.begin(), std::make_index_sequence<N>())
  {
    // clang & gcc doesn't recognize init.size() as a constexpr
    // static_assert(init.size() >= N, "Cannot initialize a carray with an smaller initializer list");
  }

  // Iterators
  constexpr iterator begin() noexcept { return data_; }
  constexpr const_iterator begin() const noexcept { return data_; }
  constexpr const_iterator cbegin() const noexcept { return data_; }
  constexpr iterator end() noexcept { return data_ + N; }
  constexpr const_iterator end() const noexcept { return data_ + N; }
  constexpr const_iterator cend() const noexcept { return data_ + N; }

  constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
  constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
  constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
  constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

  // Capacity
  constexpr size_type size() const { return N; }
  constexpr size_type max_size() const { return N; }

  // Element access
  constexpr       reference operator[](std::size_t index) { return data_[index]; }
  constexpr const_reference operator[](std::size_t index) const { return data_[index]; }

  constexpr       reference at(std::size_t index) {
    if (index > N)
      FROZEN_THROW_OR_ABORT(std::out_of_range("Index (" + std::to_string(index) + ") out of bound (" + std::to_string(N) + ')'));
    return data_[index];
  }
  constexpr const_reference at(std::size_t index) const {
    if (index > N)
      FROZEN_THROW_OR_ABORT(std::out_of_range("Index (" + std::to_string(index) + ") out of bound (" + std::to_string(N) + ')'));
    return data_[index];
  }

  constexpr       reference front() { return data_[0]; }
  constexpr const_reference front() const { return data_[0]; }

  constexpr       reference back() { return data_[N - 1]; }
  constexpr const_reference back() const { return data_[N - 1]; }

  constexpr       value_type* data() noexcept { return data_; }
  constexpr const value_type* data() const noexcept { return data_; }

  // Modifiers
  constexpr void fill(const value_type& val) {
    for (std::size_t i = 0; i < N; ++i)
      data_[i] = val;
  }
};
template <class T>
class carray<T, 0> {

public:
  // Container typdefs
  using value_type = T;
  using reference = value_type &;
  using const_reference = const value_type &;
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  // Constructors
  constexpr carray(void) = default;

};

} // namespace bits

} // namespace frozen

#endif
