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

#ifndef FROZEN_LETITGO_ELSA_H
#define FROZEN_LETITGO_ELSA_H

#include <type_traits>

namespace frozen {

template <class T = void> struct elsa {
  static_assert(std::is_integral<T>::value || std::is_enum<T>::value,
                "only supports integral types, specialize for other types");

  constexpr std::size_t operator()(T const &value, std::size_t seed) const {
    std::size_t key = seed ^ static_cast<std::size_t>(value);
    key = (~key) + (key << 21); // key = (key << 21) - key - 1;
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8); // key * 265
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4); // key * 21
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return key;
  }
};

template <> struct elsa<void> {
  template<class T>
  constexpr std::size_t operator()(T const &value, std::size_t seed) const {
    return elsa<T>{}(value, seed);
  }
};

template <class T=void> using anna = elsa<T>;
} // namespace frozen

#endif
