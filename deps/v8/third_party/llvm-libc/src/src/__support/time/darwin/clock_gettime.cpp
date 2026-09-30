//===-- Darwin implementation of internal clock_gettime -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/time/clock_gettime.h"
#include "hdr/errno_macros.h" // For EINVAL
#include "hdr/time_macros.h"
#include "hdr/types/struct_timespec.h"
#include "hdr/types/struct_timeval.h"
#include "src/__support/OSUtil/syscall.h" // For syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include <sys/syscall.h> // For SYS_gettimeofday
#include <sys/time.h>    // For struct timezone

namespace LIBC_NAMESPACE_DECL {
namespace internal {

ErrorOr<int> clock_gettime(clockid_t clockid, struct timespec *ts) {
  if (clockid != CLOCK_REALTIME)
    return Error(EINVAL);
  struct timeval tv;
  // The second argument to gettimeofday is a timezone pointer
  // The third argument is mach_absolute_time
  // Both of these, we don't need here, so they are 0
  long ret = LIBC_NAMESPACE::syscall_impl<long>(
      SYS_gettimeofday, reinterpret_cast<long>(&tv), 0, 0);
  if (ret != 0)
    // The syscall returns -1 on error and sets errno.
    return Error(EINVAL);

  ts->tv_sec = tv.tv_sec;
  ts->tv_nsec = tv.tv_usec * 1000;
  return 0;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
