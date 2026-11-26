//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Construction of Linux CPU-time clock IDs for a given process or thread.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_TIME_LINUX_CPU_CLOCK_H
#define LLVM_LIBC_SRC___SUPPORT_TIME_LINUX_CPU_CLOCK_H

#include "hdr/types/clockid_t.h"
#include "hdr/types/pid_t.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// The CPU-time clock of a specific process or thread is identified by a clock
// ID which encodes the process or thread ID. The functions below match the
// encoding used by the Linux kernel, which is not a part of the userspace API
// headers.

LIBC_INLINE_VAR constexpr unsigned int CPUCLOCK_SCHED = 2;
LIBC_INLINE_VAR constexpr unsigned int CPUCLOCK_PERTHREAD_MASK = 4;

LIBC_INLINE constexpr clockid_t make_cpu_clock(pid_t id, unsigned int flags) {
  // Shift as unsigned: ~id is negative for valid IDs, and left-shifting a
  // negative value is UB.
  return static_cast<clockid_t>((~static_cast<unsigned int>(id) << 3) | flags);
}

LIBC_INLINE constexpr clockid_t make_process_cpu_clock(pid_t pid) {
  return make_cpu_clock(pid, CPUCLOCK_SCHED);
}

LIBC_INLINE constexpr clockid_t make_thread_cpu_clock(pid_t tid) {
  return make_cpu_clock(tid, CPUCLOCK_SCHED | CPUCLOCK_PERTHREAD_MASK);
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_TIME_LINUX_CPU_CLOCK_H
