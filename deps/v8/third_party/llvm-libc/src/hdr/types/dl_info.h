//===-- Proxy for struct timespec  ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_HDR_TYPES_DLINFO_H
#define LLVM_LIBC_HDR_TYPES_DLINFO_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/Dl_info.h"

#else

#include <dlfcn.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_DLINFO_H
