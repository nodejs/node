//===-- Proxy for struct seminfo ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_STRUCT_SEMINFO_H
#define LLVM_LIBC_HDR_TYPES_STRUCT_SEMINFO_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/struct_seminfo.h"

#else // Overlay mode

#include <sys/sem.h>

#endif // LLVM_LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_STRUCT_SEMINFO_H
