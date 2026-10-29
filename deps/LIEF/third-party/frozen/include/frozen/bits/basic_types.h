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
#include "frozen/bits/constexpr_assert.h"

#include <array>
#include <utility>
#include <string>
#include <type_traits>

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
  constexpr       iterator begin() noexcept { return data; }
  constexpr       iterator end() noexcept { return data + dsize; }
  constexpr const_iterator begin() const noexcept { return data; }
  constexpr const_iterator end() const noexcept { return data + dsize; }

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

  template <class Iter, std::size_t... I>
  constexpr carray(Iter iter, std::index_sequence<I...>)
      : data_{((void)I, *iter++)...} {}
  template <std::size_t... I>
  constexpr carray(const T& value, std::index_sequence<I...>)
      : data_{((void)I, value)...} {}

  static constexpr void check_initializer(std::initializer_list<T> init) {
    (void)init;
    constexpr_assert(init.size() == N, "Cannot initialize a carray with an initializer list of different size.");
  }

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
  constexpr carray() = default;
  constexpr carray(const value_type& val)
    : carray(val, std::make_index_sequence<N>()) {}
  template <typename U, std::enable_if_t<std::is_convertible<U, T>::value, std::size_t> M>
  constexpr carray(U const (&init)[M])
    : carray(init, std::make_index_sequence<N>())
  {
    static_assert(M >= N, "Cannot initialize a carray with an smaller array");
  }
  template <typename U, std::enable_if_t<std::is_convertible<U, T>::value, std::size_t> M>
  constexpr carray(std::array<U, M> const &init)
    : carray(init.begin(), std::make_index_sequence<N>())
  {
    static_assert(M >= N, "Cannot initialize a carray with an smaller array");
  }
  template <typename U, std::enable_if_t<std::is_convertible<U, T>::value>* = nullptr>
  constexpr carray(std::initializer_list<U> init)
    : carray((check_initializer(init), init.begin()), std::make_index_sequence<N>())
  {
  }
  template <typename U, std::enable_if_t<std::is_convertible<U, T>::value>* = nullptr>
  constexpr carray(const carray<U, N>& rhs)
    : carray(rhs.begin(), std::make_index_sequence<N>())
  {
  }

  // Iterators
  constexpr iterator begin() noexcept { return data_; }
  constexpr const_iterator begin() const noexcept { return data_; }
  constexpr iterator end() noexcept { return data_ + N; }
  constexpr const_iterator end() const noexcept { return data_ + N; }

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
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  // Constructors
  constexpr carray(void) = default;

};

} // namespace bits

} // namespace frozen

#endif
