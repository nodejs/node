//===-- Definition of Linux signal number macros --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SIGNAL_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SIGNAL_MACROS_H

#include "../../__llvm-libc-common.h"

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGIOT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGSTKFLT 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGIO 29
#define SIGPOLL SIGIO
#define SIGPWR 30
#define SIGSYS 31

// Max signal number
#define NSIG 64

// SIGRTMIN is current set to the minimum usable from user mode programs. If
// the libc itself uses some of these signal numbers for private operations,
// then it has to be adjusted in future to reflect that.
#define SIGRTMIN 32

#define SIGRTMAX NSIG

// The kernel sigset is stored as an array of long values. Each bit of this
// array corresponds to a signal, adjusted by 1. That is, bit 0 corresponds
// to signal number 1, bit 1 corresponds to signal number 2 and so on. The
// below macro denotes the size of that array (in number of long words and
// not bytes).
#define __NSIGSET_WORDS (NSIG / (sizeof(unsigned long) * 8))

#define SIG_BLOCK 0   // For blocking signals
#define SIG_UNBLOCK 1 // For unblocking signals
#define SIG_SETMASK 2 // For setting signal mask

// Flag values to be used for setting sigaction.sa_flags.
#define SA_NOCLDSTOP 0x00000001
#define SA_NOCLDWAIT 0x00000002
#define SA_SIGINFO 0x00000004
#define SA_RESTART 0x10000000
#define SA_RESTORER 0x04000000
#define SA_ONSTACK 0x08000000
#define SA_NODEFER 0x40000000
#define SA_RESETHAND 0x80000000

// Signal stack flags
#define SS_ONSTACK 0x1
#define SS_DISABLE 0x2

#if defined(__x86_64__) || defined(__i386__) || defined(__riscv)
#define MINSIGSTKSZ 2048
#define SIGSTKSZ 8192
#elif defined(__aarch64__)
#define MINSIGSTKSZ 5120
#define SIGSTKSZ 16384
#else
#error "Signal stack sizes not defined for your platform."
#endif

#define SIG_ERR __LLVM_LIBC_CAST(reinterpret_cast, void (*)(int), -1)
#define SIG_DFL __LLVM_LIBC_CAST(reinterpret_cast, void (*)(int), 0)
#define SIG_IGN __LLVM_LIBC_CAST(reinterpret_cast, void (*)(int), 1)
#define SIG_HOLD __LLVM_LIBC_CAST(reinterpret_cast, void (*)(int), 2)

// SIGILL si_codes
#define ILL_ILLOPC 1 // Illegal opcode
#define ILL_ILLOPN 2 // Illegal operand
#define ILL_ILLADR 3 // Illegal addressing mode
#define ILL_ILLTRP 4 // Illegal trap
#define ILL_PRVOPC 5 // Privileged opcode
#define ILL_PRVREG 6 // Privileged register
#define ILL_COPROC 7 // Coprocessor error
#define ILL_BADSTK 8 // Internal stack error

// SIGFPE si_codes
#define FPE_INTDIV 1 // Integer divide by zero
#define FPE_INTOVF 2 // Integer overflow
#define FPE_FLTDIV 3 // Floating-point divide by zero
#define FPE_FLTOVF 4 // Floating-point overflow
#define FPE_FLTUND 5 // Floating-point underflow
#define FPE_FLTRES 6 // Floating-point inexact result
#define FPE_FLTINV 7 // Invalid floating-point operation
#define FPE_FLTSUB 8 // Subscript out of range

// SIGSEGV si_codes
#define SEGV_MAPERR 1 // Address not mapped to object
#define SEGV_ACCERR 2 // Invalid permissions for mapped object

// SIGBUS si_codes
#define BUS_ADRALN 1 // Invalid address alignment
#define BUS_ADRERR 2 // Non-existent physical address
#define BUS_OBJERR 3 // Object-specific hardware error

// SIGTRAP si_codes
#define TRAP_BRKPT 1 // Process breakpoint
#define TRAP_TRACE 2 // Process trace trap

// SIGCHLD si_codes
#define CLD_EXITED 1    // child has exited
#define CLD_KILLED 2    // child was killed
#define CLD_DUMPED 3    // child terminated abnormally
#define CLD_TRAPPED 4   // traced child has trapped
#define CLD_STOPPED 5   // child has stopped
#define CLD_CONTINUED 6 // stopped child has continued

// Other si_codes.
#define SI_USER 0       // Sent by kill()
#define SI_QUEUE (-1)   // Sent by sigqueue()
#define SI_TIMER (-2)   // Expiration of a timer set by timer_settime()
#define SI_ASYNCIO (-4) // Completion of an synchronous I/O request
#define SI_MESGQ (-3)   // Arrival of a message on an empty message queue

#endif // LLVM_LIBC_MACROS_LINUX_SIGNAL_MACROS_H
