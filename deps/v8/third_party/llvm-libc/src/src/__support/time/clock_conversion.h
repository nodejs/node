//===--- clock conversion implementation ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_TIME_CLOCK_CONVERSION_H
#define LLVM_LIBC_SRC___SUPPORT_TIME_CLOCK_CONVERSION_H

#include "src/__support/macros/config.h"
#include "src/__support/time/clock_gettime.h"
#include "src/__support/time/units.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// @brief Convert a timespec value from one clock domain to another.
//
// The function takes a timestamp that is expressed in terms of the clock
// identified by param from and returns an equivalent timestamp expressed
// in terms of the clock identified by param to.
//
// Internally it obtains the current time of both clocks with
// clock_gettime, then subtracts the source clock’s value and
// adds the target clock’s value. The result is normalised so that
// the nanoseconds field is always in the range [0, 1 s).
//
// This is useful, for example, for converting a value obtained from
// CLOCK_MONOTONIC to CLOCK_REALTIME (or vice‑versa) so that the
// timestamp can be displayed to a user or stored in a format that
// is independent of the original clock domain.
//
// @param input The timestamp to convert
// @param from Clock ID of the original timestamp (e.g. CLOCK_MONOTONIC).
// @param to Clock ID of the desired timestamp (e.g. CLOCK_REALTIME).
// @return The converted timespec
//
LIBC_INLINE timespec convert_clock(timespec input, clockid_t from,
                                   clockid_t to) {
  using namespace time_units;
  timespec from_time;
  timespec to_time;
  timespec output;
  internal::clock_gettime(from, &from_time);
  internal::clock_gettime(to, &to_time);
  output.tv_sec = input.tv_sec - from_time.tv_sec + to_time.tv_sec;
  output.tv_nsec = input.tv_nsec - from_time.tv_nsec + to_time.tv_nsec;

  if (output.tv_nsec >= 1_s_ns) {
    output.tv_sec++;
    output.tv_nsec -= 1_s_ns;
  } else if (output.tv_nsec < 0) {
    output.tv_sec--;
    output.tv_nsec += 1_s_ns;
  }
  return output;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_TIME_CLOCK_CONVERSION_H
