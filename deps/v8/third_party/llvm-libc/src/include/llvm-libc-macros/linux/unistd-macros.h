//===-- Definition of macros from unistd.h --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_UNISTD_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_UNISTD_MACROS_H

// Values for mode argument to the access(...) function.
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define _SC_PAGESIZE 1
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_CONF 83
#define _SC_NPROCESSORS_ONLN 84

#define _PC_FILESIZEBITS 0
#define _PC_LINK_MAX 1
#define _PC_MAX_CANON 2
#define _PC_MAX_INPUT 3
#define _PC_NAME_MAX 4
#define _PC_PATH_MAX 5
#define _PC_PIPE_BUF 6
#define _PC_2_SYMLINKS 7
#define _PC_ALLOC_SIZE_MIN 8
#define _PC_REC_INCR_XFER_SIZE 9
#define _PC_REC_MAX_XFER_SIZE 10
#define _PC_REC_MIN_XFER_SIZE 11
#define _PC_REC_XFER_ALIGN 12
#define _PC_SYMLINK_MAX 13
#define _PC_CHOWN_RESTRICTED 14
#define _PC_NO_TRUNC 15
#define _PC_VDISABLE 16
#define _PC_ASYNC_IO 17
#define _PC_PRIO_IO 18
#define _PC_SYNC_IO 19

// TODO: Move these limit macros to a separate file
#define _POSIX_CHOWN_RESTRICTED 1
#define _POSIX_PIPE_BUF 512
#define _POSIX_NO_TRUNC 1
#define _POSIX_VDISABLE '\0'

// Macro to set up the call to the __llvm_libc_syscall function
// This is to prevent the call from having fewer than 6 arguments, since six
// arguments are always passed to the syscall. Unnecessary arguments are
// ignored.
#define __syscall_helper(sysno, arg1, arg2, arg3, arg4, arg5, arg6, ...)       \
  __llvm_libc_syscall((long)(sysno), (long)(arg1), (long)(arg2), (long)(arg3), \
                      (long)(arg4), (long)(arg5), (long)(arg6))
#define syscall(...) __syscall_helper(__VA_ARGS__, 0, 1, 2, 3, 4, 5, 6)

#endif // LLVM_LIBC_MACROS_LINUX_UNISTD_MACROS_H
