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

#ifndef FROZEN_LETITGO_RANDOM_H
#define FROZEN_LETITGO_RANDOM_H

#include "frozen/bits/algorithms.h"
#include "frozen/bits/version.h"

#include <cstdint>
#include <type_traits>

namespace frozen {
template <class UIntType, UIntType a, UIntType c, UIntType m>
class linear_congruential_engine {

  static_assert(std::is_unsigned<UIntType>::value,
                "UIntType must be an unsigned integral type");

  template<class T>
  static constexpr UIntType modulo(T val, std::integral_constant<UIntType, 0>) {
    return static_cast<UIntType>(val);
  }

  template<class T, UIntType M>
  static constexpr UIntType modulo(T val, std::integral_constant<UIntType, M>) {
    // the static cast below may end up doing a truncation
    return static_cast<UIntType>(val % M);
  }

public:
  using result_type = UIntType;
  static constexpr result_type multiplier = a;
  static constexpr result_type increment = c;
  static constexpr result_type modulus = m;
  static constexpr result_type default_seed = 1u;

  linear_congruential_engine() = default;
  constexpr linear_congruential_engine(result_type s) { seed(s); }

  void seed(result_type s = default_seed) { state_ = s; }
  constexpr result_type operator()() {
	  using uint_least_t = bits::select_uint_least_t<bits::log(a) + bits::log(m) + 4>;
    uint_least_t tmp = static_cast<uint_least_t>(multiplier) * state_ + increment;

    state_ = modulo(tmp, std::integral_constant<UIntType, modulus>());
    return state_;
  }
  constexpr void discard(unsigned long long n) {
    while (n--)
      operator()();
  }
  static constexpr result_type min() { return increment == 0u ? 1u : 0u; }
  static constexpr result_type max() { return modulus - 1u; }
  friend constexpr bool operator==(linear_congruential_engine const &self,
                                   linear_congruential_engine const &other) {
    return self.state_ == other.state_;
  }
  friend constexpr bool operator!=(linear_congruential_engine const &self,
                                   linear_congruential_engine const &other) {
    return !(self == other);
  }

private:
  result_type state_ = default_seed;
};

using minstd_rand0 =
    linear_congruential_engine<std::uint_fast32_t, 16807, 0, 2147483647>;
using minstd_rand =
    linear_congruential_engine<std::uint_fast32_t, 48271, 0, 2147483647>;

// This generator is used by default in unordered frozen containers
using default_prg_t = minstd_rand;

} // namespace frozen

#endif
