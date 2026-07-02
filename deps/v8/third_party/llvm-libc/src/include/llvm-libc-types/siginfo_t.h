//===-- Definition of siginfo_t type --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_SIGINFO_T_H
#define LLVM_LIBC_TYPES_SIGINFO_T_H

#include "clock_t.h"
#include "pid_t.h"
#include "uid_t.h"
#include "union_sigval.h"

#define SI_MAX_SIZE 128

typedef struct {
  int si_signo; /* Signal number.  */
  int si_errno; /* If non-zero, an errno value associated with
                   this signal, as defined in <errno.h>.  */
  int si_code;  /* Signal code.  */
  union {
    int _si_pad[SI_MAX_SIZE / sizeof(int)];

    /* kill() */
    struct {
      pid_t si_pid; /* sender's pid */
      uid_t si_uid; /* sender's uid */
    } _kill;

    /* POSIX.1b timers */
    struct {
      int si_tid;             /* timer id */
      int _overrun;           /* overrun count */
      union sigval si_sigval; /* same as below */
    } _timer;

    /* POSIX.1b signals */
    struct {
      pid_t si_pid; /* sender's pid */
      uid_t si_uid; /* sender's uid */
      union sigval si_sigval;
    } _rt;

    /* SIGCHLD */
    struct {
      pid_t si_pid;  /* which child */
      uid_t si_uid;  /* sender's uid */
      int si_status; /* exit code */
      clock_t si_utime;
      clock_t si_stime;
    } _sigchld;

    /* SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTRAP, SIGEMT */
    struct {
      void *si_addr;         /* faulting insn/memory ref. */
      short int si_addr_lsb; /* Valid LSB of the reported address.  */
      union {
        /* used when si_code=SEGV_BNDERR */
        struct {
          void *_lower;
          void *_upper;
        } _addr_bnd;
        /* used when si_code=SEGV_PKUERR */
        __UINT32_TYPE__ _pkey;
      } _bounds;
    } _sigfault;

    /* SIGPOLL */
    struct {
      long int si_band; /* POLL_IN, POLL_OUT, POLL_MSG */
      int si_fd;
    } _sigpoll;

    /* SIGSYS */
    struct {
      void *_call_addr;   /* calling user insn */
      int _syscall;       /* triggering system call number */
      unsigned int _arch; /* AUDIT_ARCH_* of syscall */
    } _sigsys;
  } _sifields;
} siginfo_t;

#undef SI_MAX_SIZE

#define si_pid _sifields._kill.si_pid
#define si_uid _sifields._kill.si_uid
#define si_timerid _sifields._timer.si_tid
#define si_overrun _sifields._timer.si_overrun
#define si_status _sifields._sigchld.si_status
#define si_utime _sifields._sigchld.si_utime
#define si_stime _sifields._sigchld.si_stime
#define si_value _sifields._rt.si_sigval
#define si_int _sifields._rt.si_sigval.sival_int
#define si_ptr _sifields._rt.si_sigval.sival_ptr
#define si_addr _sifields._sigfault.si_addr
#define si_addr_lsb _sifields._sigfault.si_addr_lsb
#define si_lower _sifields._sigfault._bounds._addr_bnd._lower
#define si_upper _sifields._sigfault._bounds._addr_bnd._upper
#define si_pkey _sifields._sigfault._bounds._pkey
#define si_band _sifields._sigpoll.si_band
#define si_fd _sifields._sigpoll.si_fd
#define si_call_addr _sifields._sigsys._call_addr
#define si_syscall _sifields._sigsys._syscall
#define si_arch _sifields._sigsys._arch

#endif // LLVM_LIBC_TYPES_SIGINFO_T_H
