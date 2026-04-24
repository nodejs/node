//
// Copyright 2026 The Abseil Authors.
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

#ifdef _WIN32
#include <intrin.h>
// kFastFailRangeCheckFailure mirrors the FAST_FAIL_RANGE_CHECK_FAILURE macro.
// Typically FAST_FAIL_RANGE_CHECK_FAILURE would be imported from winnt.h
// but winnt.h pulls in other dependencies and introduces build failures.
constexpr unsigned int kFastFailRangeCheckFailure = 8u;
#endif

#include "absl/base/internal/hardening.h"

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/macros.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace base_internal {

[[noreturn]] ABSL_ATTRIBUTE_NOINLINE void FailedBoundsCheckAbort() {
#ifdef _WIN32
  __fastfail(kFastFailRangeCheckFailure);
#else
  ABSL_INTERNAL_HARDENING_ABORT();
#endif
}

}  // namespace base_internal

ABSL_NAMESPACE_END
}  // namespace absl
