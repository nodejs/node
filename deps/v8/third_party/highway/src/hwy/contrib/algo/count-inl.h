// Copyright 2026 Google LLC
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

#include <stddef.h>
#include <stdint.h>

// Per-target include guard
#if defined(HIGHWAY_HWY_CONTRIB_ALGO_COUNT_INL_H_) == \
    defined(HWY_TARGET_TOGGLE)  // NOLINT
#ifdef HIGHWAY_HWY_CONTRIB_ALGO_COUNT_INL_H_
#undef HIGHWAY_HWY_CONTRIB_ALGO_COUNT_INL_H_
#else
#define HIGHWAY_HWY_CONTRIB_ALGO_COUNT_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// Returns the number of elements in `in[0, count)` equal to `value`.
template <class D, typename T = TFromD<D>>
size_t Count(D d, T value, const T* HWY_RESTRICT in, size_t count) {
  const size_t N = Lanes(d);
  using V = Vec<D>;
  const V broadcasted = Set(d, value);
  const RebindToSigned<D> di;
  using TI = TFromD<decltype(di)>;
  using VI = Vec<decltype(di)>;

  size_t total = 0;
  size_t i = 0;

  // Min 4 lanes needed for two pairwise widenings, 8->16->32
  if constexpr (sizeof(T) == 1 && HWY_MAX_LANES_D(D) >= 4) {
    const VI k1 = Set(di, TI{1});
    const RebindToUnsigned<decltype(di)> du;
    const RepartitionToWide<decltype(di)> di16;
    const Repartition<int32_t, D> di32;
    auto wide_sum = Zero(di32);

    if (count >= 4 * N && N >= 4) {
      while (i <= count - 4 * N) {
        VI acc0 = Zero(di);
        VI acc1 = Zero(di);
        VI acc2 = Zero(di);
        VI acc3 = Zero(di);
        const size_t cap = HWY_MIN(i + 128 * 4 * N, count);

        if constexpr (HWY_NATIVE_MASK) {
          for (; i <= cap - 4 * N; i += 4 * N) {
            const auto m0 = RebindMask(di, Eq(broadcasted, LoadU(d, in + i)));
            const auto m1 =
                RebindMask(di, Eq(broadcasted, LoadU(d, in + i + N)));
            const auto m2 =
                RebindMask(di, Eq(broadcasted, LoadU(d, in + i + 2 * N)));
            const auto m3 =
                RebindMask(di, Eq(broadcasted, LoadU(d, in + i + 3 * N)));
            acc0 = MaskedAddOr(acc0, m0, acc0, k1);
            acc1 = MaskedAddOr(acc1, m1, acc1, k1);
            acc2 = MaskedAddOr(acc2, m2, acc2, k1);
            acc3 = MaskedAddOr(acc3, m3, acc3, k1);
          }
        } else {
          for (; i <= cap - 4 * N; i += 4 * N) {
            const auto v0 = VecFromMask(d, Eq(broadcasted, LoadU(d, in + i)));
            const auto v1 =
                VecFromMask(d, Eq(broadcasted, LoadU(d, in + i + N)));
            const auto v2 =
                VecFromMask(d, Eq(broadcasted, LoadU(d, in + i + 2 * N)));
            const auto v3 =
                VecFromMask(d, Eq(broadcasted, LoadU(d, in + i + 3 * N)));
            acc0 = Add(acc0, BitCast(di, v0));
            acc1 = Add(acc1, BitCast(di, v1));
            acc2 = Add(acc2, BitCast(di, v2));
            acc3 = Add(acc3, BitCast(di, v3));
          }

          acc0 = Neg(acc0);
          acc1 = Neg(acc1);
          acc2 = Neg(acc2);
          acc3 = Neg(acc3);
        }

        const auto w0 = SatWidenMulPairwiseAdd(di16, BitCast(du, acc0), k1);
        const auto w1 = SatWidenMulPairwiseAdd(di16, BitCast(du, acc1), k1);
        const auto w2 = SatWidenMulPairwiseAdd(di16, BitCast(du, acc2), k1);
        const auto w3 = SatWidenMulPairwiseAdd(di16, BitCast(du, acc3), k1);
        const auto sum16 = Add(Add(w0, w1), Add(w2, w3));
        wide_sum = SatWidenMulPairwiseAccumulate(
            di32, sum16, Set(di16, int16_t{1}), wide_sum);
      }
    }
    total += static_cast<size_t>(ReduceSum(di32, wide_sum));
  } else if constexpr (sizeof(T) == 2 && HWY_MAX_LANES_D(D) >= 2) {
    // Min 2 lanes needed for pairwise widening, 16->32
    const Repartition<int32_t, D> di32;
    auto wide_sum = Zero(di32);

    if (count >= 4 * N && N >= 2) {
      while (i <= count - 4 * N) {
        VI acc0 = Zero(di);
        VI acc1 = Zero(di);
        VI acc2 = Zero(di);
        VI acc3 = Zero(di);
        const size_t cap = HWY_MIN(i + 32768 * 4 * N, count);

        if constexpr (HWY_NATIVE_MASK) {
          const auto k1 = Set(di, TI{1});
          for (; i <= cap - 4 * N; i += 4 * N) {
            const auto m0 = RebindMask(di, Eq(broadcasted, LoadU(d, in + i)));
            const auto m1 =
                RebindMask(di, Eq(broadcasted, LoadU(d, in + i + N)));
            const auto m2 =
                RebindMask(di, Eq(broadcasted, LoadU(d, in + i + 2 * N)));
            const auto m3 =
                RebindMask(di, Eq(broadcasted, LoadU(d, in + i + 3 * N)));
            acc0 = MaskedAddOr(acc0, m0, acc0, k1);
            acc1 = MaskedAddOr(acc1, m1, acc1, k1);
            acc2 = MaskedAddOr(acc2, m2, acc2, k1);
            acc3 = MaskedAddOr(acc3, m3, acc3, k1);
          }
        } else {
          for (; i <= cap - 4 * N; i += 4 * N) {
            const auto v0 = VecFromMask(d, Eq(broadcasted, LoadU(d, in + i)));
            const auto v1 =
                VecFromMask(d, Eq(broadcasted, LoadU(d, in + i + N)));
            const auto v2 =
                VecFromMask(d, Eq(broadcasted, LoadU(d, in + i + 2 * N)));
            const auto v3 =
                VecFromMask(d, Eq(broadcasted, LoadU(d, in + i + 3 * N)));
            acc0 = Add(acc0, BitCast(di, v0));
            acc1 = Add(acc1, BitCast(di, v1));
            acc2 = Add(acc2, BitCast(di, v2));
            acc3 = Add(acc3, BitCast(di, v3));
          }
        }
        const auto mul = Set(di, HWY_NATIVE_MASK ? TI{1} : TI{-1});
        wide_sum = SatWidenMulPairwiseAccumulate(di32, acc0, mul, wide_sum);
        wide_sum = SatWidenMulPairwiseAccumulate(di32, acc1, mul, wide_sum);
        wide_sum = SatWidenMulPairwiseAccumulate(di32, acc2, mul, wide_sum);
        wide_sum = SatWidenMulPairwiseAccumulate(di32, acc3, mul, wide_sum);
      }
    }
    total += static_cast<size_t>(ReduceSum(di32, wide_sum));
  } else {
    // Lane type wide enough to accumulate directly
    if (count >= 4 * N) {
      VI acc0 = Zero(di);
      VI acc1 = Zero(di);
      VI acc2 = Zero(di);
      VI acc3 = Zero(di);

      if constexpr (HWY_NATIVE_MASK) {
        const auto k1 = Set(di, TI{1});
        for (; i <= count - 4 * N; i += 4 * N) {
          const auto m0 = RebindMask(di, Eq(broadcasted, LoadU(d, in + i)));
          const auto m1 = RebindMask(di, Eq(broadcasted, LoadU(d, in + i + N)));
          const auto m2 =
              RebindMask(di, Eq(broadcasted, LoadU(d, in + i + 2 * N)));
          const auto m3 =
              RebindMask(di, Eq(broadcasted, LoadU(d, in + i + 3 * N)));
          acc0 = MaskedAddOr(acc0, m0, acc0, k1);
          acc1 = MaskedAddOr(acc1, m1, acc1, k1);
          acc2 = MaskedAddOr(acc2, m2, acc2, k1);
          acc3 = MaskedAddOr(acc3, m3, acc3, k1);
        }
        acc0 = Add(Add(acc0, acc1), Add(acc2, acc3));
      } else {
        for (; i <= count - 4 * N; i += 4 * N) {
          const auto v0 = VecFromMask(d, Eq(broadcasted, LoadU(d, in + i)));
          const auto v1 = VecFromMask(d, Eq(broadcasted, LoadU(d, in + i + N)));
          const auto v2 =
              VecFromMask(d, Eq(broadcasted, LoadU(d, in + i + 2 * N)));
          const auto v3 =
              VecFromMask(d, Eq(broadcasted, LoadU(d, in + i + 3 * N)));
          acc0 = Add(acc0, BitCast(di, v0));
          acc1 = Add(acc1, BitCast(di, v1));
          acc2 = Add(acc2, BitCast(di, v2));
          acc3 = Add(acc3, BitCast(di, v3));
        }
        acc0 = Neg(Add(Add(acc0, acc1), Add(acc2, acc3)));
      }
      total += static_cast<size_t>(ReduceSum(di, acc0));
    }
  }

  if (count >= N) {
    for (; i <= count - N; i += N) {
      total += CountTrue(d, Eq(broadcasted, LoadU(d, in + i)));
    }
  }

  if (i != count) {
    const size_t remaining = count - i;
    HWY_DASSERT(0 != remaining && remaining < N);
    const V v = LoadN(d, in + i, remaining);
    total += CountTrue(d, And(Eq(broadcasted, v), FirstN(d, remaining)));
  }

  return total;
}

// Returns the number of elements in `in[0, count)` for which `func(d, vec)`
// returns true.
template <class D, class Func, typename T = TFromD<D>>
size_t CountIf(D d, const T* HWY_RESTRICT in, size_t count, const Func& func) {
  const size_t N = Lanes(d);
  using V = Vec<D>;
  const RebindToSigned<D> di;
  using TI = TFromD<decltype(di)>;
  using VI = Vec<decltype(di)>;
  using MI = Mask<decltype(di)>;

  size_t total = 0;
  size_t i = 0;

  // Min 4 lanes needed for two pairwise widenings, 8->16->32
  if constexpr (sizeof(T) == 1 && HWY_MAX_LANES_D(D) >= 4) {
    const VI k1 = Set(di, TI{1});
    const RebindToUnsigned<decltype(di)> du;
    const RepartitionToWide<decltype(di)> di16;
    const Repartition<int32_t, D> di32;
    using VI16 = Vec<decltype(di16)>;
    using VI32 = Vec<decltype(di32)>;
    VI32 wide_sum = Zero(di32);

    if (count >= 4 * N && N >= 4) {
      while (i <= count - 4 * N) {
        VI acc0 = Zero(di);
        VI acc1 = Zero(di);
        VI acc2 = Zero(di);
        VI acc3 = Zero(di);
        const size_t cap = HWY_MIN(i + 128 * 4 * N, count);

        if constexpr (HWY_NATIVE_MASK) {
          for (; i <= cap - 4 * N; i += 4 * N) {
            const MI m0 = RebindMask(di, func(d, LoadU(d, in + i)));
            const MI m1 = RebindMask(di, func(d, LoadU(d, in + i + N)));
            const MI m2 = RebindMask(di, func(d, LoadU(d, in + i + 2 * N)));
            const MI m3 = RebindMask(di, func(d, LoadU(d, in + i + 3 * N)));
            acc0 = MaskedAddOr(acc0, m0, acc0, k1);
            acc1 = MaskedAddOr(acc1, m1, acc1, k1);
            acc2 = MaskedAddOr(acc2, m2, acc2, k1);
            acc3 = MaskedAddOr(acc3, m3, acc3, k1);
          }
        } else {
          for (; i <= cap - 4 * N; i += 4 * N) {
            const V v0 = VecFromMask(d, func(d, LoadU(d, in + i)));
            const V v1 = VecFromMask(d, func(d, LoadU(d, in + i + N)));
            const V v2 = VecFromMask(d, func(d, LoadU(d, in + i + 2 * N)));
            const V v3 = VecFromMask(d, func(d, LoadU(d, in + i + 3 * N)));
            acc0 = Add(acc0, BitCast(di, v0));
            acc1 = Add(acc1, BitCast(di, v1));
            acc2 = Add(acc2, BitCast(di, v2));
            acc3 = Add(acc3, BitCast(di, v3));
          }

          acc0 = Neg(acc0);
          acc1 = Neg(acc1);
          acc2 = Neg(acc2);
          acc3 = Neg(acc3);
        }

        const VI16 w0 = SatWidenMulPairwiseAdd(di16, BitCast(du, acc0), k1);
        const VI16 w1 = SatWidenMulPairwiseAdd(di16, BitCast(du, acc1), k1);
        const VI16 w2 = SatWidenMulPairwiseAdd(di16, BitCast(du, acc2), k1);
        const VI16 w3 = SatWidenMulPairwiseAdd(di16, BitCast(du, acc3), k1);
        const VI16 sum16 = Add(Add(w0, w1), Add(w2, w3));
        wide_sum = SatWidenMulPairwiseAccumulate(
            di32, sum16, Set(di16, int16_t{1}), wide_sum);
      }
    }
    total += static_cast<size_t>(ReduceSum(di32, wide_sum));
  } else if constexpr (sizeof(T) == 2 && HWY_MAX_LANES_D(D) >= 2) {
    // Min 2 lanes needed for pairwise widening, 16->32
    const Repartition<int32_t, D> di32;
    using VI32 = Vec<decltype(di32)>;
    VI32 wide_sum = Zero(di32);

    if (count >= 4 * N && N >= 2) {
      while (i <= count - 4 * N) {
        VI acc0 = Zero(di);
        VI acc1 = Zero(di);
        VI acc2 = Zero(di);
        VI acc3 = Zero(di);
        const size_t cap = HWY_MIN(i + 32768 * 4 * N, count);

        if constexpr (HWY_NATIVE_MASK) {
          const VI k1 = Set(di, TI{1});
          for (; i <= cap - 4 * N; i += 4 * N) {
            const MI m0 = RebindMask(di, func(d, LoadU(d, in + i)));
            const MI m1 = RebindMask(di, func(d, LoadU(d, in + i + N)));
            const MI m2 = RebindMask(di, func(d, LoadU(d, in + i + 2 * N)));
            const MI m3 = RebindMask(di, func(d, LoadU(d, in + i + 3 * N)));
            acc0 = MaskedAddOr(acc0, m0, acc0, k1);
            acc1 = MaskedAddOr(acc1, m1, acc1, k1);
            acc2 = MaskedAddOr(acc2, m2, acc2, k1);
            acc3 = MaskedAddOr(acc3, m3, acc3, k1);
          }
        } else {
          for (; i <= cap - 4 * N; i += 4 * N) {
            const V v0 = VecFromMask(d, func(d, LoadU(d, in + i)));
            const V v1 = VecFromMask(d, func(d, LoadU(d, in + i + N)));
            const V v2 = VecFromMask(d, func(d, LoadU(d, in + i + 2 * N)));
            const V v3 = VecFromMask(d, func(d, LoadU(d, in + i + 3 * N)));
            acc0 = Add(acc0, BitCast(di, v0));
            acc1 = Add(acc1, BitCast(di, v1));
            acc2 = Add(acc2, BitCast(di, v2));
            acc3 = Add(acc3, BitCast(di, v3));
          }
        }
        const VI mul = Set(di, HWY_NATIVE_MASK ? TI{1} : TI{-1});
        wide_sum = SatWidenMulPairwiseAccumulate(di32, acc0, mul, wide_sum);
        wide_sum = SatWidenMulPairwiseAccumulate(di32, acc1, mul, wide_sum);
        wide_sum = SatWidenMulPairwiseAccumulate(di32, acc2, mul, wide_sum);
        wide_sum = SatWidenMulPairwiseAccumulate(di32, acc3, mul, wide_sum);
      }
    }
    total += static_cast<size_t>(ReduceSum(di32, wide_sum));
  } else {
    // Lane type wide enough to accumulate directly
    if (count >= 4 * N) {
      VI acc0 = Zero(di);
      VI acc1 = Zero(di);
      VI acc2 = Zero(di);
      VI acc3 = Zero(di);

      if constexpr (HWY_NATIVE_MASK) {
        const VI k1 = Set(di, TI{1});
        for (; i <= count - 4 * N; i += 4 * N) {
          const MI m0 = RebindMask(di, func(d, LoadU(d, in + i)));
          const MI m1 = RebindMask(di, func(d, LoadU(d, in + i + N)));
          const MI m2 = RebindMask(di, func(d, LoadU(d, in + i + 2 * N)));
          const MI m3 = RebindMask(di, func(d, LoadU(d, in + i + 3 * N)));
          acc0 = MaskedAddOr(acc0, m0, acc0, k1);
          acc1 = MaskedAddOr(acc1, m1, acc1, k1);
          acc2 = MaskedAddOr(acc2, m2, acc2, k1);
          acc3 = MaskedAddOr(acc3, m3, acc3, k1);
        }
        acc0 = Add(Add(acc0, acc1), Add(acc2, acc3));
      } else {
        for (; i <= count - 4 * N; i += 4 * N) {
          const V v0 = VecFromMask(d, func(d, LoadU(d, in + i)));
          const V v1 = VecFromMask(d, func(d, LoadU(d, in + i + N)));
          const V v2 = VecFromMask(d, func(d, LoadU(d, in + i + 2 * N)));
          const V v3 = VecFromMask(d, func(d, LoadU(d, in + i + 3 * N)));
          acc0 = Add(acc0, BitCast(di, v0));
          acc1 = Add(acc1, BitCast(di, v1));
          acc2 = Add(acc2, BitCast(di, v2));
          acc3 = Add(acc3, BitCast(di, v3));
        }
        acc0 = Neg(Add(Add(acc0, acc1), Add(acc2, acc3)));
      }
      total += static_cast<size_t>(ReduceSum(di, acc0));
    }
  }

  if (count >= N) {
    for (; i <= count - N; i += N) {
      total += CountTrue(d, func(d, LoadU(d, in + i)));
    }
  }

  if (i != count) {
    const size_t remaining = count - i;
    HWY_DASSERT(0 != remaining && remaining < N);
    const V v = LoadN(d, in + i, remaining);
    total += CountTrue(d, And(func(d, v), FirstN(d, remaining)));
  }

  return total;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_ALGO_COUNT_INL_H_
