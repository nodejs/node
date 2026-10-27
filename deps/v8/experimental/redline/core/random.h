// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_RANDOM_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_RANDOM_H_

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace redline {

// Deterministic pseudo random generator (xoshiro256**). Every worker and
// every command owns one, seeded from the run seed, so that a run can be
// replayed from its event log.
class Random {
 public:
  explicit Random(uint64_t seed) { Seed(seed); }

  void Seed(uint64_t seed) {
    // splitmix64 to spread the seed over the state.
    for (uint64_t& word : state_) {
      seed += 0x9E3779B97F4A7C15ull;
      uint64_t z = seed;
      z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
      z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
      word = z ^ (z >> 31);
    }
  }

  uint64_t Next() {
    const uint64_t result = Rotl(state_[1] * 5, 7) * 9;
    const uint64_t t = state_[1] << 17;
    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= t;
    state_[3] = Rotl(state_[3], 45);
    return result;
  }

  // Uniform in [0, n). n == 0 returns 0.
  uint64_t Uniform(uint64_t n) {
    if (n == 0) {
      return 0;
    }
    // Lemire's nearly divisionless method.
    uint64_t l = 0;
    uint64_t h = MulHigh(Next(), n, &l);
    if (l < n) {
      uint64_t t = (0 - n) % n;
      while (l < t) {
        h = MulHigh(Next(), n, &l);
      }
    }
    return h;
  }
  size_t Index(size_t n) { return static_cast<size_t>(Uniform(n)); }
  // Uniform in [lo, hi] (inclusive).
  int64_t Range(int64_t lo, int64_t hi) {
    if (hi <= lo) {
      return lo;
    }
    const uint64_t span = static_cast<uint64_t>(hi) - static_cast<uint64_t>(lo);
    if (span == UINT64_MAX) {
      return static_cast<int64_t>(Next());
    }
    return static_cast<int64_t>(static_cast<uint64_t>(lo) + Uniform(span + 1));
  }
  // Uniform in [0, 1).
  double Double() { return static_cast<double>(Next() >> 11) * 0x1.0p-53; }
  bool Chance(double probability) { return Double() < probability; }
  bool Coin() { return (Next() & 1) != 0; }

  template <typename T>
  const T& Pick(const std::vector<T>& items) {
    return items[Index(items.size())];
  }
  template <typename T>
  T& Pick(std::vector<T>& items) {
    return items[Index(items.size())];
  }
  template <typename T>
  T Pick(std::initializer_list<T> items) {
    return *(items.begin() + Index(items.size()));
  }
  // Index drawn with the given (non-negative) weights.
  size_t Weighted(const std::vector<double>& weights) {
    double total = 0;
    size_t last_positive = 0;
    for (size_t i = 0; i < weights.size(); i++) {
      total += weights[i];
      if (weights[i] > 0) {
        last_positive = i;
      }
    }
    if (total <= 0) {
      return Index(weights.size());
    }
    double r = Double() * total;
    for (size_t i = 0; i < weights.size(); i++) {
      if (r < weights[i]) {
        return i;
      }
      r -= weights[i];
    }
    return last_positive;
  }
  // Geometric-ish small count: 0 with probability 1-p, else 1 + ...
  int SmallCount(int max, double continue_probability = 0.5) {
    int n = 0;
    while (n < max && Chance(continue_probability)) {
      n++;
    }
    return n;
  }

  // A fresh generator derived from this one (for sub-tasks that must not
  // disturb the main stream).
  Random Fork() { return Random(Next()); }

 private:
  static uint64_t Rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
  // Full 64 x 64 -> 128 bit product: returns the high 64 bits and writes
  // the low 64 bits to *lo. The portable branch is for 32-bit targets
  // (ia32, arm), which have no __uint128_t. Both branches give the same
  // result, so a seed gives the same run on every target.
  static uint64_t MulHigh(uint64_t a, uint64_t b, uint64_t* lo) {
#if defined(__SIZEOF_INT128__)
    const __uint128_t m = static_cast<__uint128_t>(a) * b;
    *lo = static_cast<uint64_t>(m);
    return static_cast<uint64_t>(m >> 64);
#else
    const uint64_t a_lo = a & 0xFFFFFFFFull;
    const uint64_t a_hi = a >> 32;
    const uint64_t b_lo = b & 0xFFFFFFFFull;
    const uint64_t b_hi = b >> 32;
    const uint64_t p0 = a_lo * b_lo;
    const uint64_t p1 = a_lo * b_hi;
    const uint64_t p2 = a_hi * b_lo;
    const uint64_t mid =
        (p0 >> 32) + (p1 & 0xFFFFFFFFull) + (p2 & 0xFFFFFFFFull);
    *lo = (mid << 32) | (p0 & 0xFFFFFFFFull);
    return a_hi * b_hi + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
#endif
  }
  uint64_t state_[4];
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_RANDOM_H_
