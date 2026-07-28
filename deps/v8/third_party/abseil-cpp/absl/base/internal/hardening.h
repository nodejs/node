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

#include <cstddef>

#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/base/options.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace base_internal {

void SetAbslHardeningEnabled(bool enabled);

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
    ABSL_INTERNAL_HARDENING_ABORT();
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
    ABSL_INTERNAL_HARDENING_ABORT();
  }
#endif
}

template <typename T1, typename T2>
constexpr void HardeningAssertGT(T1 val1, T2 val2) {
  ABSL_ASSERT(val1 > val2);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (!ABSL_PREDICT_TRUE(val1 > val2)) {
    ABSL_INTERNAL_HARDENING_ABORT();
  }
#endif
}

template <typename T1, typename T2>
constexpr void HardeningAssertGE(T1 val1, T2 val2) {
  ABSL_ASSERT(val1 >= val2);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (!ABSL_PREDICT_TRUE(val1 >= val2)) {
    ABSL_INTERNAL_HARDENING_ABORT();
  }
#endif
}

template <typename T1, typename T2>
constexpr void HardeningAssertLT(T1 val1, T2 val2) {
  ABSL_ASSERT(val1 < val2);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (!ABSL_PREDICT_TRUE(val1 < val2)) {
    ABSL_INTERNAL_HARDENING_ABORT();
  }
#endif
}

template <typename T1, typename T2>
constexpr void HardeningAssertLE(T1 val1, T2 val2) {
  ABSL_ASSERT(val1 <= val2);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (!ABSL_PREDICT_TRUE(val1 <= val2)) {
    ABSL_INTERNAL_HARDENING_ABORT();
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
    ABSL_INTERNAL_HARDENING_ABORT();
  }
#endif
}

template <typename T>
constexpr void HardeningAssertNonNull(T ptr) {
  ABSL_ASSERT(ptr != nullptr);
#if (ABSL_OPTION_HARDENED == 1 || ABSL_OPTION_HARDENED == 2) && defined(NDEBUG)
  if (ABSL_PREDICT_FALSE(ptr == nullptr)) {
    ABSL_INTERNAL_HARDENING_ABORT();
  }
#endif
}

class ScopedSetAbslHardeningForTesting {
 private:
  bool prev_state_;

 public:
  explicit ScopedSetAbslHardeningForTesting([[maybe_unused]] bool enabled) {
    prev_state_ = false;
    SetAbslHardeningEnabled(enabled);
  }
  ~ScopedSetAbslHardeningForTesting() {
    absl::base_internal::SetAbslHardeningEnabled(prev_state_);
  }
};

}  // namespace base_internal

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_BASE_INTERNAL_HARDENING_H_
