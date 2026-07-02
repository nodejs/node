//===-- Definition of Linux signal number macros --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SIGNAL_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SIGNAL_MACROS_H

#include "__llvm-libc-common.h"

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

// SIGCHLD si_codes
#define CLD_EXITED 1    // child has exited
#define CLD_KILLED 2    // child was killed
#define CLD_DUMPED 3    // child terminated abnormally
#define CLD_TRAPPED 4   // traced child has trapped
#define CLD_STOPPED 5   // child has stopped
#define CLD_CONTINUED 6 // stopped child has continued

#endif // LLVM_LIBC_MACROS_LINUX_SIGNAL_MACROS_H
