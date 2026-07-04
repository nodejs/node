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
#include <stdio.h>

#include <numeric>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE \
  "hwy/examples/sum_array_simple.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

float SumArraySIMD(const float* HWY_RESTRICT array, size_t count) {
  const hn::ScalableTag<float> d;
  using V = hn::Vec<decltype(d)>;
  V sum = hn::Zero(d);
  size_t i = 0;
  const size_t N = hn::Lanes(d);
  if (count >= N) {
    for (; i <= count - N; i += N) {
      sum = hn::Add(sum, hn::LoadU(d, array + i));
    }
  }
  float total = hn::ReduceSum(d, sum);
  // Simple scalar remainder handling
  for (; i < count; ++i) {
    total += array[i];
  }
  return total;
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
HWY_EXPORT(SumArraySIMD);

float CallSumArraySIMD(const float* array, size_t count) {
  return HWY_DYNAMIC_DISPATCH(SumArraySIMD)(array, count);
}

}  // namespace hwy

int main() {
  const size_t count = 1025;
  std::vector<float> data(count);
  std::iota(data.begin(), data.end(), 1.0f);

  float sum = hwy::CallSumArraySIMD(data.data(), count);
  printf("Sum: %f\n", sum);

  return 0;
}
#endif  // HWY_ONCE
