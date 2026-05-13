//===-- Proxy for clockid_t -----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_CLOCKID_T_H
#define LLVM_LIBC_HDR_TYPES_CLOCKID_T_H

// TODO: we will need to decide how to export extension to windows.
#if defined(LIBC_FULL_BUILD) || defined(_WIN32)

#include "include/llvm-libc-types/clockid_t.h"

#else // Overlay mode

#include <sys/types.h>

#endif // LLVM_LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_CLOCKID_T_H
