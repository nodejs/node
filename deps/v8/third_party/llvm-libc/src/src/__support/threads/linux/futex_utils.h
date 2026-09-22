//===--- Futex Wrapper ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_FUTEX_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_FUTEX_UTILS_H

#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/OSUtil/syscall.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/linux/futex_word.h"
#include "src/__support/time/abs_timeout.h"
#include <linux/errno.h>
#include <linux/futex.h>
#if defined(SYS_futex_time64)
#include <linux/time_types.h>
#endif

namespace LIBC_NAMESPACE_DECL {

#if !defined(SYS_futex_time64) && defined(SYS_futex)
static_assert(
    sizeof(timespec::tv_nsec) == sizeof(long),
    "This legacy syscall fallback is only safe on platforms where tv_nsec "
    "matches the register size (long). It is unsafe on 32-bit platforms "
    "with 64-bit tv_nsec.");
#endif

class Futex : public cpp::Atomic<FutexWordType> {
public:
  using Timeout = internal::AbsTimeout;
  LIBC_INLINE constexpr Futex(FutexWordType value)
      : cpp::Atomic<FutexWordType>(value) {}
  LIBC_INLINE Futex &operator=(FutexWordType value) {
    cpp::Atomic<FutexWordType>::store(value);
    return *this;
  }
  LIBC_INLINE ErrorOr<int> wait(FutexWordType expected,
                                cpp::optional<Timeout> timeout = cpp::nullopt,
                                bool is_shared = false) {
    // use bitset variants to enforce abs_time
    uint32_t op = is_shared ? FUTEX_WAIT_BITSET : FUTEX_WAIT_BITSET_PRIVATE;
    if (timeout && timeout->is_realtime()) {
      op |= FUTEX_CLOCK_REALTIME;
    }
    for (;;) {
      if (this->load(cpp::MemoryOrder::RELAXED) != expected)
        return 0;

      void *timeout_ptr = nullptr;
#if defined(SYS_futex_time64)
      __kernel_timespec ts64;
      if (timeout) {
        // If timespec has the same size and layout as __kernel_timespec, we can
        // pass a pointer to it directly.
        if constexpr (sizeof(timespec) == sizeof(__kernel_timespec)) {
          timeout_ptr = const_cast<timespec *>(&timeout->get_timespec());
        } else {
          // Otherwise (e.g. 32-bit platforms with 32-bit time_t), the 64-bit
          // futex syscall (SYS_futex_time64) still expects a 64-bit timespec.
          // We must convert it to avoid passing a mismatching structure size.
          ts64.tv_sec = timeout->get_timespec().tv_sec;
          ts64.tv_nsec = timeout->get_timespec().tv_nsec;
          timeout_ptr = &ts64;
        }
      }
#else
      timespec ts;
      if (timeout) {
        ts = timeout->get_timespec();
        timeout_ptr = &ts;
      }
#endif

      int ret = syscall_impl<int>(
          /*syscall_number=*/FUTEX_SYSCALL_ID,
          /*futex_addr=*/this,
          /*op=*/op,
          /*expected=*/expected,
          /*timeout=*/timeout_ptr,
          /*ignored=*/nullptr,
          /*bitset=*/FUTEX_BITSET_MATCH_ANY);

      // continue waiting if interrupted; otherwise return the result
      // which should normally be 0 or -ETIMEOUT.
      if (ret == -EINTR)
        continue;

      // EAGAIN and EWOULDBLOCK will be treated as a normal finish, as the value
      // has changed.
      if (ret == -EAGAIN || ret == -EWOULDBLOCK)
        return 0;

      if (ret < 0)
        return cpp::unexpected(-ret);
      return ret;
    }
  }
  LIBC_INLINE ErrorOr<int> notify_one(bool is_shared = false) {
    int ret = syscall_impl<int>(
        /*syscall_number=*/FUTEX_SYSCALL_ID,
        /*futex_addr=*/this,
        /*op=*/is_shared ? FUTEX_WAKE : FUTEX_WAKE_PRIVATE,
        /*wake_limit=*/1,
        /*ignored=*/nullptr,
        /*ignored=*/nullptr,
        /* ignored */ 0);
    if (ret < 0)
      return cpp::unexpected(-ret);
    return ret;
  }
  LIBC_INLINE ErrorOr<int> notify_all(bool is_shared = false) {
    int ret = syscall_impl<int>(
        /*syscall_number=*/FUTEX_SYSCALL_ID,
        /*futex_addr=*/this,
        /*op=*/is_shared ? FUTEX_WAKE : FUTEX_WAKE_PRIVATE,
        /*wake_limit=*/cpp::numeric_limits<int>::max(),
        /*ignored=*/nullptr,
        /*ignored=*/nullptr,
        /*ignored=*/0);
    if (ret < 0)
      return cpp::unexpected(-ret);
    return ret;
  }
  LIBC_INLINE ErrorOr<int> requeue_to(Futex &other,
                                      cpp::optional<FutexWordType> oldval,
                                      int wake_limit, int requeue_limit,
                                      bool is_shared = false) {
    int ret;
    if (oldval)
      ret = syscall_impl<int>(
          /*syscall_number=*/FUTEX_SYSCALL_ID,
          /*futex_addr=*/this,
          /*op=*/
          is_shared ? FUTEX_CMP_REQUEUE : FUTEX_CMP_REQUEUE_PRIVATE,
          /*wake_limit=*/wake_limit,
          /*requeue_limit=*/requeue_limit,
          /*requeue_addr=*/&other, *oldval);
    else
      ret = syscall_impl<int>(
          /*syscall_number=*/FUTEX_SYSCALL_ID,
          /*futex_addr=*/this,
          /*op=*/is_shared ? FUTEX_REQUEUE : FUTEX_REQUEUE_PRIVATE,
          /*wake_limit=*/wake_limit,
          /*requeue_limit=*/requeue_limit,
          /*requeue_addr=*/&other);
    if (ret < 0)
      return cpp::unexpected<int>(-ret);
    return static_cast<int>(ret);
  }
};

static_assert(__is_standard_layout(Futex),
              "Futex must be a standard layout type.");
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_FUTEX_UTILS_H
