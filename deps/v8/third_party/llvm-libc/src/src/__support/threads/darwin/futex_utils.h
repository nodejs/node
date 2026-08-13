//===--- Futex utils for Darwin ----------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_DARWIN_FUTEX_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_DARWIN_FUTEX_UTILS_H

#include "hdr/errno_macros.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/error_or.h"
#include "src/__support/time/abs_timeout.h"
#include "src/__support/time/clock_conversion.h"
#include "src/__support/time/units.h"

#include <os/os_sync_wait_on_address.h>

namespace LIBC_NAMESPACE_DECL {

using FutexWordType = uint32_t;

// errno from libSystem
extern "C" int *__error(void);

class FutexErrnoProtect {
  int backup;

public:
  LIBC_INLINE FutexErrnoProtect() : backup(*__error()) { *__error() = 0; }
  LIBC_INLINE ~FutexErrnoProtect() { *__error() = backup; }
};

struct Futex : public cpp::Atomic<FutexWordType> {
  using cpp::Atomic<FutexWordType>::Atomic;
  using Timeout = internal::AbsTimeout;

  LIBC_INLINE ErrorOr<int>
  wait(FutexWordType val, cpp::optional<Timeout> timeout, bool is_shared) {
    FutexErrnoProtect protect;
    os_sync_wait_on_address_flags_t flags = OS_SYNC_WAIT_ON_ADDRESS_NONE;
    if (is_shared)
      flags = OS_SYNC_WAIT_ON_ADDRESS_SHARED;
    for (;;) {
      if (this->load(cpp::MemoryOrder::RELAXED) != val)
        return 0;
      int ret = 0;
      if (timeout) {
        // Assuming, OS_CLOCK_MACH_ABSOLUTE_TIME is equivalent to CLOCK_REALTIME
        using namespace time_units;
        uint64_t tnsec = timeout->get_timespec().tv_sec * 1_s_ns +
                         timeout->get_timespec().tv_nsec;
        ret = os_sync_wait_on_address_with_timeout(
            reinterpret_cast<void *>(this), static_cast<uint64_t>(val),
            sizeof(FutexWordType), flags, OS_CLOCK_MACH_ABSOLUTE_TIME, tnsec);
      } else {
        ret = os_sync_wait_on_address(reinterpret_cast<void *>(this),
                                      static_cast<uint64_t>(val),
                                      sizeof(FutexWordType), flags);
      }
      if ((ret < 0) && (*__error() == ETIMEDOUT))
        return cpp::unexpected(ETIMEDOUT);
      // case when os_sync returns early with an error. retry.
      if ((ret < 0) && ((*__error() == EINTR) || (*__error() == EFAULT)))
        continue;
      return ret;
    }
  }

  LIBC_INLINE ErrorOr<int> notify_one(bool is_shared) {
    FutexErrnoProtect protect;
    os_sync_wake_by_address_flags_t flags = OS_SYNC_WAKE_BY_ADDRESS_NONE;
    if (is_shared)
      flags = OS_SYNC_WAKE_BY_ADDRESS_SHARED;
    int res = os_sync_wake_by_address_any(reinterpret_cast<void *>(this),
                                          sizeof(FutexWordType), flags);
    if (res < 0)
      return cpp::unexpected(*__error());
    return res;
  }

  LIBC_INLINE ErrorOr<int> notify_all(bool is_shared) {
    FutexErrnoProtect protect;
    os_sync_wake_by_address_flags_t flags = OS_SYNC_WAKE_BY_ADDRESS_NONE;
    if (is_shared)
      flags = OS_SYNC_WAKE_BY_ADDRESS_SHARED;
    int res = os_sync_wake_by_address_all(reinterpret_cast<void *>(this),
                                          sizeof(FutexWordType), flags);
    if (res < 0)
      return cpp::unexpected(*__error());
    return res;
  }

  LIBC_INLINE ErrorOr<int> requeue_to(Futex & /*other*/,
                                      cpp::optional<FutexWordType> /*oldval*/,
                                      int /*wake_limit*/, int /*requeue_limit*/,
                                      bool /*is_shared*/ = false) {
    return cpp::unexpected(ENOSYS);
  }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_DARWIN_FUTEX_UTILS_H
