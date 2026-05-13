//===------------- Linux VDSO Symbols ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "hdr/types/clock_t.h"
#include "hdr/types/clockid_t.h"
#include "hdr/types/struct_timespec.h"
#include "hdr/types/struct_timeval.h"
#include "hdr/types/time_t.h"
#include "src/__support/common.h"
#include <stddef.h> // For size_t.

// NOLINTBEGIN(llvmlibc-implementation-in-namespace)
// TODO: some of the following can be defined via proxy headers.
struct __kernel_timespec;
struct timezone;
struct riscv_hwprobe;
struct getcpu_cache;
struct cpu_set_t;
// NOLINTEND(llvmlibc-implementation-in-namespace)

namespace LIBC_NAMESPACE_DECL {
namespace vdso {

enum class VDSOSym {
  ClockGetTime,
  ClockGetTime64,
  GetTimeOfDay,
  GetCpu,
  Time,
  ClockGetRes,
  RTSigReturn,
  FlushICache,
  RiscvHwProbe,
  GetRandom,
  VDSOSymCount,
};

template <VDSOSym sym> LIBC_INLINE constexpr auto dispatcher() {
  if constexpr (sym == VDSOSym::ClockGetTime)
    return static_cast<int (*)(clockid_t, timespec *)>(nullptr);
  else if constexpr (sym == VDSOSym::ClockGetTime64)
    return static_cast<int (*)(clockid_t, __kernel_timespec *)>(nullptr);
  else if constexpr (sym == VDSOSym::GetTimeOfDay)
    return static_cast<int (*)(timeval *__restrict,
                               struct timezone *__restrict)>(nullptr);
  else if constexpr (sym == VDSOSym::GetCpu)
    return static_cast<int (*)(unsigned *, unsigned *, getcpu_cache *)>(
        nullptr);
  else if constexpr (sym == VDSOSym::Time)
    return static_cast<time_t (*)(time_t *)>(nullptr);
  else if constexpr (sym == VDSOSym::ClockGetRes)
    return static_cast<int (*)(clockid_t, timespec *)>(nullptr);
  else if constexpr (sym == VDSOSym::RTSigReturn)
    return static_cast<void (*)(void)>(nullptr);
  else if constexpr (sym == VDSOSym::FlushICache)
    return static_cast<void (*)(void *, void *, unsigned int)>(nullptr);
  else if constexpr (sym == VDSOSym::RiscvHwProbe)
    return static_cast<int (*)(riscv_hwprobe *, size_t, size_t, cpu_set_t *,
                               unsigned)>(nullptr);
  else if constexpr (sym == VDSOSym::GetRandom)
    return static_cast<int (*)(void *, size_t, unsigned int, void *, size_t)>(
        nullptr);
  else
    return static_cast<void *>(nullptr);
}

template <VDSOSym sym> using VDSOSymType = decltype(dispatcher<sym>());

} // namespace vdso
} // namespace LIBC_NAMESPACE_DECL
