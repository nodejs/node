//===-- Definition of macros from errno.h ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_ERRNO_MACROS_H
#define LLVM_LIBC_HDR_ERRNO_MACROS_H

#ifdef LIBC_FULL_BUILD

#ifdef __linux__
#include <linux/errno.h>

#include "include/llvm-libc-macros/error-number-macros.h"
#elif defined(__APPLE__)
#include <sys/errno.h>
#else // __APPLE__
#include "include/llvm-libc-macros/generic-error-number-macros.h"
#endif

#else // Overlay mode

#include <errno.h>

#endif // LLVM_LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_ERRNO_MACROS_H
