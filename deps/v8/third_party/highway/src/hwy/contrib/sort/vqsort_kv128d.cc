// Copyright 2021 Google LLC
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

#include "hwy/contrib/sort/vqsort.h"  // VQSort

#undef HWY_TARGET_INCLUDE
// clang-format off
// (avoid line break, which would prevent Copybara rules from matching)
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/vqsort_kv128d.cc"  //NOLINT
// clang-format on
#include "hwy/foreach_target.h"  // IWYU pragma: keep

// After foreach_target
#include "hwy/contrib/sort/vqsort-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

void SortKV128Desc(K64V64* HWY_RESTRICT keys, const size_t num) {
  // 128-bit keys require 128-bit SIMD.
#if HWY_TARGET != HWY_SCALAR
  return VQSortStatic(keys, num, SortDescending());
#else
  (void)keys;
  (void)num;
#endif
}

void PartialSortKV128Desc(K64V64* HWY_RESTRICT keys, const size_t num,
                          const size_t k) {
  // 128-bit keys require 128-bit SIMD.
#if HWY_TARGET != HWY_SCALAR
  return VQPartialSortStatic(keys, num, k, SortDescending());
#else
  (void)keys;
  (void)num;
  (void)k;
#endif
}

void SelectKV128Desc(K64V64* HWY_RESTRICT keys, const size_t num,
                     const size_t k) {
  // 128-bit keys require 128-bit SIMD.
#if HWY_TARGET != HWY_SCALAR
  return VQSelectStatic(keys, num, k, SortDescending());
#else
  (void)keys;
  (void)num;
  (void)k;
#endif
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_EXPORT(SortKV128Desc);
HWY_EXPORT(PartialSortKV128Desc);
HWY_EXPORT(SelectKV128Desc);
}  // namespace

void VQSort(K64V64* HWY_RESTRICT keys, const size_t n, SortDescending) {
  HWY_DYNAMIC_DISPATCH(SortKV128Desc)(keys, n);
}

void VQPartialSort(K64V64* HWY_RESTRICT keys, const size_t n, const size_t k,
                   SortDescending) {
  HWY_DYNAMIC_DISPATCH(PartialSortKV128Desc)(keys, n, k);
}

void VQSelect(K64V64* HWY_RESTRICT keys, const size_t n, const size_t k,
              SortDescending) {
  HWY_DYNAMIC_DISPATCH(SelectKV128Desc)(keys, n, k);
}

void Sorter::operator()(K64V64* HWY_RESTRICT keys, size_t n,
                        SortDescending tag) const {
  VQSort(keys, n, tag);
}

}  // namespace hwy
#endif  // HWY_ONCE
