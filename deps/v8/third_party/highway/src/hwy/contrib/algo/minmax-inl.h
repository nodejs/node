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

// Per-target include guard
#if defined(HIGHWAY_HWY_CONTRIB_ALGO_MINMAX_INL_H_) == \
    defined(HWY_TARGET_TOGGLE)  // NOLINT
#ifdef HIGHWAY_HWY_CONTRIB_ALGO_MINMAX_INL_H_
#undef HIGHWAY_HWY_CONTRIB_ALGO_MINMAX_INL_H_
#else
#define HIGHWAY_HWY_CONTRIB_ALGO_MINMAX_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// Returns the minimum value in `in[0, count)` or PositiveInfOrHighestValue<T>() if count == 0.
template <class D, typename T = TFromD<D>>
T MinValue(D d, const T* HWY_RESTRICT in, size_t count) {
  const size_t N = Lanes(d);
  const T identity = hwy::PositiveInfOrHighestValue<T>();
  const Vec<D> identity_vec = Set(d, identity);

  Vec<D> acc0 = identity_vec;
  Vec<D> acc1 = identity_vec;
  Vec<D> acc2 = identity_vec;
  Vec<D> acc3 = identity_vec;

  size_t i = 0;
  if (count >= 4 * N) {
    for (; i <= count - 4 * N; i += 4 * N) {
      acc0 = Min(acc0, LoadU(d, in + i));
      acc1 = Min(acc1, LoadU(d, in + i + N));
      acc2 = Min(acc2, LoadU(d, in + i + 2 * N));
      acc3 = Min(acc3, LoadU(d, in + i + 3 * N));
    }
  }

  acc0 = Min(Min(acc0, acc1), Min(acc2, acc3));

  for (; i < count; i += N) {
    const size_t remaining = count - i;
    const size_t n = HWY_MIN(remaining, N);
    acc0 = Min(acc0, LoadNOr(identity_vec, d, in + i, n));
  }

  return ReduceMin(d, acc0);
}

// Returns the maximum value in `in[0, count)` or NegativeInfOrLowestValue<T>() if count == 0.
template <class D, typename T = TFromD<D>>
T MaxValue(D d, const T* HWY_RESTRICT in, size_t count) {
  const size_t N = Lanes(d);
  const T identity = hwy::NegativeInfOrLowestValue<T>();
  const Vec<D> identity_vec = Set(d, identity);

  Vec<D> acc0 = identity_vec;
  Vec<D> acc1 = identity_vec;
  Vec<D> acc2 = identity_vec;
  Vec<D> acc3 = identity_vec;

  size_t i = 0;
  if (count >= 4 * N) {
    for (; i <= count - 4 * N; i += 4 * N) {
      acc0 = Max(acc0, LoadU(d, in + i));
      acc1 = Max(acc1, LoadU(d, in + i + N));
      acc2 = Max(acc2, LoadU(d, in + i + 2 * N));
      acc3 = Max(acc3, LoadU(d, in + i + 3 * N));
    }
  }

  acc0 = Max(Max(acc0, acc1), Max(acc2, acc3));

  for (; i < count; i += N) {
    const size_t remaining = count - i;
    const size_t n = HWY_MIN(remaining, N);
    acc0 = Max(acc0, LoadNOr(identity_vec, d, in + i, n));
  }

  return ReduceMax(d, acc0);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_ALGO_MINMAX_INL_H_
