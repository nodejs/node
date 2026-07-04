//===--- timeout implementation ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_TIME_MONOTONICITY_H
#define LLVM_LIBC_SRC___SUPPORT_TIME_MONOTONICITY_H

#include "hdr/time_macros.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/time/abs_timeout.h"
#include "src/__support/time/clock_conversion.h"
namespace LIBC_NAMESPACE_DECL {
namespace internal {
// This function is separated from abs_timeout.
// This function pulls in the dependency to clock_conversion.h,
// which may transitively depend on vDSO hence futex. However, this structure
// would be passed to futex, so we need to avoid cyclic dependencies.
// This function is going to be used in timed locks. Pthread generally uses
// realtime clocks for timeouts. However, due to non-monotoncity, realtime
// clocks reportedly lead to undesired behaviors. Therefore, we also provide a
// method to convert the timespec to a monotonic clock relative to the time of
// function call.
LIBC_INLINE void ensure_monotonicity(AbsTimeout &timeout) {
  if (timeout.is_realtime()) {
    auto res = AbsTimeout::from_timespec(
        convert_clock(timeout.get_timespec(), CLOCK_REALTIME, CLOCK_MONOTONIC),
        false);

    // Clamp the timeout to epoch if becomes negative after the conversion.
    if (!res.has_value() && res.error() == AbsTimeout::Error::BeforeEpoch)
      res = AbsTimeout::from_timespec(timespec{0, 0}, false);

    LIBC_ASSERT(res.has_value());
    if (!res.has_value())
      __builtin_unreachable();

    timeout = *res;
  }
}
} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_TIME_MONOTONICITY_H
