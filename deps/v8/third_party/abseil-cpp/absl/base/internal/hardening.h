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
// This header file defines macros and functions for performing Abseil
// hardening checks and aborts.

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

[[noreturn]] ABSL_ATTRIBUTE_NOINLINE void HardeningAbort();

// `HardeningAssert` performs runtime checks when Abseil Hardening is enabled,
// even if `NDEBUG` is defined.
//
// When `NDEBUG` is not defined, `HardeningAssert`'s behavior is identical to
// `ABSL_ASSERT`.
//
// Prefer a more specific assertion function over this more general one,
// as assertion functions which perform the comparison themselves
// can have the cost of the comparison attributed to them.
constexpr void HardeningAssert(bool cond) {
  ABSL_ASSERT(cond);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (ABSL_PREDICT_FALSE(!cond)) {
    ABSL_INTERNAL_ATTRIBUTE_NO_MERGE HardeningAbort();
  }
#endif
}

// `HardeningAssertSlow` is used to perform runtime checks which are too
// computationally expensive to enable widely by default.
//
// When `NDEBUG` is not defined, `HardeningAssertSlow`'s behavior is identical
// to `ABSL_ASSERT`.
constexpr void HardeningAssertSlow(bool cond) {
  ABSL_ASSERT(cond);
#if (ABSL_OPTION_HARDENED == 1) && defined(NDEBUG)
  if (ABSL_PREDICT_FALSE(!cond)) {
    ABSL_INTERNAL_ATTRIBUTE_NO_MERGE HardeningAbort();
  }
#endif
}

template <typename T>
constexpr void HardeningAssertGT(T val1, T val2) {
  ABSL_ASSERT(val1 > val2);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (!ABSL_PREDICT_TRUE(val1 > val2)) {
    ABSL_INTERNAL_ATTRIBUTE_NO_MERGE HardeningAbort();
  }
#endif
}

template <typename T>
constexpr void HardeningAssertGE(T val1, T val2) {
  ABSL_ASSERT(val1 >= val2);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (!ABSL_PREDICT_TRUE(val1 >= val2)) {
    ABSL_INTERNAL_ATTRIBUTE_NO_MERGE HardeningAbort();
  }
#endif
}

template <typename T>
constexpr void HardeningAssertLT(T val1, T val2) {
  ABSL_ASSERT(val1 < val2);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (!ABSL_PREDICT_TRUE(val1 < val2)) {
    ABSL_INTERNAL_ATTRIBUTE_NO_MERGE HardeningAbort();
  }
#endif
}

template <typename T>
constexpr void HardeningAssertLE(T val1, T val2) {
  ABSL_ASSERT(val1 <= val2);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (!ABSL_PREDICT_TRUE(val1 <= val2)) {
    ABSL_INTERNAL_ATTRIBUTE_NO_MERGE HardeningAbort();
  }
#endif
}

constexpr void HardeningAssertInBounds(size_t index, size_t size) {
  HardeningAssertLT(index, size);
}

template <typename T>
constexpr void HardeningAssertNonEmpty(const T& container) {
  ABSL_ASSERT(!container.empty());
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (ABSL_PREDICT_FALSE(container.empty())) {
    ABSL_INTERNAL_ATTRIBUTE_NO_MERGE HardeningAbort();
  }
#endif
}

template <typename T>
constexpr void HardeningAssertNonNull(T ptr) {
  ABSL_ASSERT(ptr != nullptr);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (ABSL_PREDICT_FALSE(ptr == nullptr)) {
    ABSL_INTERNAL_ATTRIBUTE_NO_MERGE HardeningAbort();
  }
#endif
}

}  // namespace base_internal

ABSL_NAMESPACE_END
}  // namespace absl

#undef ABSL_INTERNAL_ATTRIBUTE_NO_MERGE

#endif  // ABSL_BASE_INTERNAL_HARDENING_H_
