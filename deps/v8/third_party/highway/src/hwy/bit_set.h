// Copyright 2024 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HIGHWAY_HWY_BIT_SET_H_
#define HIGHWAY_HWY_BIT_SET_H_

// Various BitSet for 64, up to 4096, or any number of bits.

#include <stddef.h>

#include <atomic>

#include "hwy/base.h"

namespace hwy {

// 64-bit specialization of `std::bitset`, which lacks `Foreach`.
class BitSet64 {
 public:
  constexpr size_t MaxSize() const { return 64; }

  // No harm if `i` is already set.
  void Set(size_t i) {
    HWY_DASSERT(i < 64);
    bits_ |= (1ULL << i);
    HWY_DASSERT(Get(i));
  }

  // Equivalent to Set(i) for i in [0, 64) where (bits >> i) & 1. This does
  // not clear any existing bits.
  void SetNonzeroBitsFrom64(uint64_t bits) { bits_ |= bits; }

  void Clear(size_t i) {
    HWY_DASSERT(i < 64);
    bits_ &= ~(1ULL << i);
  }

  bool Get(size_t i) const {
    HWY_DASSERT(i < 64);
    return (bits_ & (1ULL << i)) != 0;
  }

  // Returns true if Get(i) would return true for any i in [0, 64).
  bool Any() const { return bits_ != 0; }

  // Returns true if Get(i) would return true for all i in [0, 64).
  bool All() const { return bits_ == ~uint64_t{0}; }

  // Returns lowest i such that `Get(i)`. Caller must first ensure `Any()`!
  size_t First() const {
    HWY_DASSERT(Any());
    return Num0BitsBelowLS1Bit_Nonzero64(bits_);
  }

  // Returns lowest i such that `!Get(i)`. Caller must first ensure `!All()`!
  size_t First0() const {
    HWY_DASSERT(!All());
    return Num0BitsBelowLS1Bit_Nonzero64(~bits_);
  }

  // Returns uint64_t(Get(i)) << i for i in [0, 64).
  uint64_t Get64() const { return bits_; }

  // Calls `func(i)` for each `i` in the set. It is safe for `func` to modify
  // the set, but the current Foreach call is unaffected.
  template <class Func>
  void Foreach(const Func& func) const {
    uint64_t remaining_bits = bits_;
    while (remaining_bits != 0) {
      const size_t i = Num0BitsBelowLS1Bit_Nonzero64(remaining_bits);
      remaining_bits &= remaining_bits - 1;  // clear LSB
      func(i);
    }
  }

  size_t Count() const { return PopCount(bits_); }

 private:
  uint64_t bits_ = 0;
};

// Any number of bits, flat array.
template <size_t kMaxSize>
class BitSet {
  static_assert(kMaxSize != 0, "BitSet requires non-zero size");

 public:
  constexpr size_t MaxSize() const { return kMaxSize; }

  // No harm if `i` is already set.
  void Set(size_t i) {
    HWY_DASSERT(i < kMaxSize);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    bits_[idx].Set(mod);
  }

  void Clear(size_t i) {
    HWY_DASSERT(i < kMaxSize);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    bits_[idx].Clear(mod);
    HWY_DASSERT(!Get(i));
  }

  bool Get(size_t i) const {
    HWY_DASSERT(i < kMaxSize);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    return bits_[idx].Get(mod);
  }

  // Returns true if Get(i) would return true for any i in [0, kMaxSize).
  bool Any() const {
    for (const BitSet64& bits : bits_) {
      if (bits.Any()) return true;
    }
    return false;
  }

  // Returns true if Get(i) would return true for all i in [0, kMaxSize).
  bool All() const {
    for (size_t idx = 0; idx < kNum64 - 1; ++idx) {
      if (!bits_[idx].All()) return false;
    }

    constexpr size_t kRemainder = kMaxSize % 64;
    if (kRemainder == 0) {
      return bits_[kNum64 - 1].All();
    }
    return bits_[kNum64 - 1].Count() == kRemainder;
  }

  // Returns lowest i such that `Get(i)`. Caller must first ensure `Any()`!
  size_t First() const {
    HWY_DASSERT(Any());
    for (size_t idx = 0;; ++idx) {
      HWY_DASSERT(idx < kNum64);
      if (bits_[idx].Any()) return idx * 64 + bits_[idx].First();
    }
  }

  // Returns lowest i such that `!Get(i)`. Caller must first ensure `All()`!
  size_t First0() const {
    HWY_DASSERT(!All());
    for (size_t idx = 0;; ++idx) {
      HWY_DASSERT(idx < kNum64);
      if (!bits_[idx].All()) {
        const size_t first0 = idx * 64 + bits_[idx].First0();
        HWY_DASSERT(first0 < kMaxSize);
        return first0;
      }
    }
  }

  // Calls `func(i)` for each `i` in the set. It is safe for `func` to modify
  // the set, but the current Foreach call is only affected if changing one of
  // the not yet visited BitSet64.
  template <class Func>
  void Foreach(const Func& func) const {
    for (size_t idx = 0; idx < kNum64; ++idx) {
      bits_[idx].Foreach([idx, &func](size_t mod) { func(idx * 64 + mod); });
    }
  }

  size_t Count() const {
    size_t total = 0;
    for (const BitSet64& bits : bits_) {
      total += bits.Count();
    }
    return total;
  }

 private:
  static constexpr size_t kNum64 = DivCeil(kMaxSize, size_t{64});
  BitSet64 bits_[kNum64];
};

// Any number of bits, flat array, atomic updates to the u64.
template <size_t kMaxSize>
class AtomicBitSet {
  static_assert(kMaxSize != 0, "AtomicBitSet requires non-zero size");

  // Bits may signal something to other threads, hence relaxed is insufficient.
  // Acq/Rel ensures a happens-before relationship.
  static constexpr auto kAcq = std::memory_order_acquire;
  static constexpr auto kRel = std::memory_order_release;

 public:
  constexpr size_t MaxSize() const { return kMaxSize; }

  // No harm if `i` is already set.
  void Set(size_t i) {
    HWY_DASSERT(i < kMaxSize);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    bits_[idx].fetch_or(1ULL << mod, kRel);
  }

  void Clear(size_t i) {
    HWY_DASSERT(i < kMaxSize);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    bits_[idx].fetch_and(~(1ULL << mod), kRel);
    HWY_DASSERT(!Get(i));
  }

  bool Get(size_t i) const {
    HWY_DASSERT(i < kMaxSize);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    return ((bits_[idx].load(kAcq) & (1ULL << mod))) != 0;
  }

  // Returns true if Get(i) would return true for any i in [0, kMaxSize).
  bool Any() const {
    for (const std::atomic<uint64_t>& bits : bits_) {
      if (bits.load(kAcq)) return true;
    }
    return false;
  }

  // Returns true if Get(i) would return true for all i in [0, kMaxSize).
  bool All() const {
    for (size_t idx = 0; idx < kNum64 - 1; ++idx) {
      if (bits_[idx].load(kAcq) != ~uint64_t{0}) return false;
    }

    constexpr size_t kRemainder = kMaxSize % 64;
    const uint64_t last_bits = bits_[kNum64 - 1].load(kAcq);
    if (kRemainder == 0) {
      return last_bits == ~uint64_t{0};
    }
    return PopCount(last_bits) == kRemainder;
  }

  // Returns lowest i such that `Get(i)`. Caller must first ensure `Any()`!
  size_t First() const {
    HWY_DASSERT(Any());
    for (size_t idx = 0;; ++idx) {
      HWY_DASSERT(idx < kNum64);
      const uint64_t bits = bits_[idx].load(kAcq);
      if (bits != 0) {
        return idx * 64 + Num0BitsBelowLS1Bit_Nonzero64(bits);
      }
    }
  }

  // Returns lowest i such that `!Get(i)`. Caller must first ensure `!All()`!
  size_t First0() const {
    HWY_DASSERT(!All());
    for (size_t idx = 0;; ++idx) {
      HWY_DASSERT(idx < kNum64);
      const uint64_t inv_bits = ~bits_[idx].load(kAcq);
      if (inv_bits != 0) {
        const size_t first0 =
            idx * 64 + Num0BitsBelowLS1Bit_Nonzero64(inv_bits);
        HWY_DASSERT(first0 < kMaxSize);
        return first0;
      }
    }
  }

  // Calls `func(i)` for each `i` in the set. It is safe for `func` to modify
  // the set, but the current Foreach call is only affected if changing one of
  // the not yet visited uint64_t.
  template <class Func>
  void Foreach(const Func& func) const {
    for (size_t idx = 0; idx < kNum64; ++idx) {
      uint64_t remaining_bits = bits_[idx].load(kAcq);
      while (remaining_bits != 0) {
        const size_t i = Num0BitsBelowLS1Bit_Nonzero64(remaining_bits);
        remaining_bits &= remaining_bits - 1;  // clear LSB
        func(idx * 64 + i);
      }
    }
  }

  size_t Count() const {
    size_t total = 0;
    for (const std::atomic<uint64_t>& bits : bits_) {
      total += PopCount(bits.load(kAcq));
    }
    return total;
  }

 private:
  static constexpr size_t kNum64 = DivCeil(kMaxSize, size_t{64});
  std::atomic<uint64_t> bits_[kNum64] = {};
};

// Two-level bitset for up to `kMaxSize` <= 4096 values. The iterators
// (`Any/First/Foreach/Count`) are more efficient than `BitSet` for sparse sets.
// This comes at the cost of slightly slower mutators (`Set/Clear`).
template <size_t kMaxSize = 4096>
class BitSet4096 {
  static_assert(kMaxSize != 0, "BitSet4096 requires non-zero size");

 public:
  constexpr size_t MaxSize() const { return kMaxSize; }

  // No harm if `i` is already set.
  void Set(size_t i) {
    HWY_DASSERT(i < kMaxSize);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    bits_[idx].Set(mod);
    nonzero_.Set(idx);
    HWY_DASSERT(Get(i));
  }

  // Equivalent to Set(i) for i in [0, 64) where (bits >> i) & 1. This does
  // not clear any existing bits.
  void SetNonzeroBitsFrom64(uint64_t bits) {
    bits_[0].SetNonzeroBitsFrom64(bits);
    if (bits) nonzero_.Set(0);
  }

  void Clear(size_t i) {
    HWY_DASSERT(i < kMaxSize);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    bits_[idx].Clear(mod);
    if (!bits_[idx].Any()) {
      nonzero_.Clear(idx);
    }
    HWY_DASSERT(!Get(i));
  }

  bool Get(size_t i) const {
    HWY_DASSERT(i < kMaxSize);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    return bits_[idx].Get(mod);
  }

  // Returns true if `Get(i)` would return true for any i in [0, kMaxSize).
  bool Any() const { return nonzero_.Any(); }

  // Returns true if `Get(i)` would return true for all i in [0, kMaxSize).
  bool All() const {
    // Do not check `nonzero_.All()` - that only works if `kMaxSize` is 4096.
    if (nonzero_.Count() != kNum64) return false;
    return Count() == kMaxSize;
  }

  // Returns lowest i such that `Get(i)`. Caller must first ensure `Any()`!
  size_t First() const {
    HWY_DASSERT(Any());
    const size_t idx = nonzero_.First();
    return idx * 64 + bits_[idx].First();
  }

  // Returns lowest i such that `!Get(i)`. Caller must first ensure `!All()`!
  size_t First0() const {
    HWY_DASSERT(!All());
    // It is likely not worthwhile to have a separate `BitSet64` for `not_all_`,
    // hence iterate over all u64.
    for (size_t idx = 0;; ++idx) {
      HWY_DASSERT(idx < kNum64);
      if (!bits_[idx].All()) {
        const size_t first0 = idx * 64 + bits_[idx].First0();
        HWY_DASSERT(first0 < kMaxSize);
        return first0;
      }
    }
  }

  // Returns uint64_t(Get(i)) << i for i in [0, 64).
  uint64_t Get64() const { return bits_[0].Get64(); }

  // Calls `func(i)` for each `i` in the set. It is safe for `func` to modify
  // the set, but the current Foreach call is only affected if changing one of
  // the not yet visited BitSet64 for which Any() is true.
  template <class Func>
  void Foreach(const Func& func) const {
    nonzero_.Foreach([&func, this](size_t idx) {
      bits_[idx].Foreach([idx, &func](size_t mod) { func(idx * 64 + mod); });
    });
  }

  size_t Count() const {
    size_t total = 0;
    nonzero_.Foreach(
        [&total, this](size_t idx) { total += bits_[idx].Count(); });
    return total;
  }

 private:
  static_assert(kMaxSize <= 64 * 64, "One BitSet64 insufficient");
  static constexpr size_t kNum64 = DivCeil(kMaxSize, size_t{64});
  BitSet64 nonzero_;
  BitSet64 bits_[kNum64];
};

}  // namespace hwy

#endif  // HIGHWAY_HWY_BIT_SET_H_
