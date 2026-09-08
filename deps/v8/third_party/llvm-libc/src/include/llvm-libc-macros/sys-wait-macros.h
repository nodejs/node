//===-- Macros defined in sys/wait.h header file --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_SYS_WAIT_MACROS_H
#define LLVM_LIBC_MACROS_SYS_WAIT_MACROS_H

#ifdef __linux__
#include "linux/sys-wait-macros.h"
#endif

#endif // LLVM_LIBC_MACROS_SYS_WAIT_MACROS_H
