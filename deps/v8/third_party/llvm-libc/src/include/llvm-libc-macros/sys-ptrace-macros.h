//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of macros from sys/ptrace.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_SYS_PTRACE_MACROS_H
#define LLVM_LIBC_MACROS_SYS_PTRACE_MACROS_H

#include <linux/ptrace.h>

// Userspace expects a different name for these two
#define PTRACE_POKEUSER PTRACE_POKEUSR
#define PTRACE_PEEKUSER PTRACE_PEEKUSR

// For BSD compatibility
#define PT_TRACE_ME PTRACE_TRACEME
#define PT_READ_I PTRACE_PEEKTEXT
#define PT_READ_D PTRACE_PEEKDATA
#define PT_WRITE_I PTRACE_POKETEXT
#define PT_WRITE_D PTRACE_POKEDATA
#define PT_CONTINUE PTRACE_CONT
#define PT_STEP PTRACE_SINGLESTEP
#define PT_KILL PTRACE_KILL
#define PT_ATTACH PTRACE_ATTACH
#define PT_DETACH PTRACE_DETACH
#define PT_SYSCALL PTRACE_SYSCALL

#endif // LLVM_LIBC_MACROS_SYS_PTRACE_MACROS_H
