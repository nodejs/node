//===-- Definition of macros from sys/auxv.h ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_SYS_AUXV_MACROS_H
#define LLVM_LIBC_HDR_SYS_AUXV_MACROS_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-macros/sys-auxv-macros.h"

#else // Overlay mode

#include <sys/auxv.h>

#endif // LLVM_LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_SYS_AUXV_MACROS_H
