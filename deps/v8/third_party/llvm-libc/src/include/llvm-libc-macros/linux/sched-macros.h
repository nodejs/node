//===-- Definition of macros from sched.h ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SCHED_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SCHED_MACROS_H

// Definitions of SCHED_* macros must match was linux as at:
// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/sched.h

// Posix required
#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2

// Linux extentions
#define SCHED_BATCH 3
#define SCHED_ISO 4 // Not yet implemented, reserved.
#define SCHED_IDLE 5
#define SCHED_DEADLINE 6

#define CPU_SETSIZE __CPU_SETSIZE
#define NCPUBITS __NCPUBITS
#define CPU_COUNT_S(setsize, set) __sched_getcpucount(setsize, set)
#define CPU_COUNT(set) CPU_COUNT_S(sizeof(cpu_set_t), set)
#define CPU_ZERO_S(setsize, set) __sched_setcpuzero(setsize, set)
#define CPU_ZERO(set) CPU_ZERO_S(sizeof(cpu_set_t), set)
#define CPU_SET_S(cpu, setsize, set) __sched_setcpuset(cpu, setsize, set)
#define CPU_SET(cpu, set) CPU_SET_S(cpu, sizeof(cpu_set_t), set)
#define CPU_ISSET_S(cpu, setsize, set) __sched_getcpuisset(cpu, setsize, set)
#define CPU_ISSET(cpu, set) CPU_ISSET_S(cpu, sizeof(cpu_set_t), set)

#endif // LLVM_LIBC_MACROS_LINUX_SCHED_MACROS_H
