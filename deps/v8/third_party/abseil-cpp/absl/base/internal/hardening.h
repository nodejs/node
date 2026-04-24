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
//
// -----------------------------------------------------------------------------
// File: hardening.h
// -----------------------------------------------------------------------------
//
// This header file defines macros and functions for checking bounds on
// accesses to Abseil container types.

#ifndef ABSL_BASE_INTERNAL_HARDENING_H_
#define ABSL_BASE_INTERNAL_HARDENING_H_

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/base/options.h"

#ifdef ABSL_INTERNAL_ATTRIBUTE_NO_MERGE
#error ABSL_INTERNAL_ATTRIBUTE_NO_MERGE cannot be directly set
#elif ABSL_HAVE_CPP_ATTRIBUTE(clang::nomerge)
#define ABSL_INTERNAL_ATTRIBUTE_NO_MERGE [[clang::nomerge]]
#else
#define ABSL_INTERNAL_ATTRIBUTE_NO_MERGE
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace base_internal {

[[noreturn]] ABSL_ATTRIBUTE_NOINLINE void FailedBoundsCheckAbort();

inline void HardeningAssertInBounds(size_t index, size_t size) {
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (ABSL_PREDICT_FALSE(index >= size)) {
    ABSL_INTERNAL_ATTRIBUTE_NO_MERGE FailedBoundsCheckAbort();
  }
#else
  ABSL_ASSERT(index < size);
#endif
}

}  // namespace base_internal

ABSL_NAMESPACE_END
}  // namespace absl

#undef ABSL_INTERNAL_ATTRIBUTE_NO_MERGE

#endif  // ABSL_BASE_INTERNAL_HARDENING_H_
