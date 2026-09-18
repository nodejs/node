// Copyright 2026 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/base/internal/cpu_detect.h"

#include "gtest/gtest.h"
#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {
namespace {

TEST(CpuDetectTest, SupportsBmi2) {
#if defined(__x86_64__) || defined(_M_X64)
#if ABSL_HAVE_BUILTIN(__builtin_cpu_supports) && defined(__linux__)
  EXPECT_EQ(SupportsBmi2(), __builtin_cpu_supports("bmi2") != 0);
#else
  // If __builtin_cpu_supports is not available, we just verify it doesn't
  // crash.
  (void)SupportsBmi2();
#endif
#else
  // On non-x86, SupportsBmi2() must return false.
  EXPECT_FALSE(SupportsBmi2());
#endif
}

}  // namespace
}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl
