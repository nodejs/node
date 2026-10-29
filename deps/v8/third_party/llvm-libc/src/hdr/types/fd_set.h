//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy for fd_set.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_FD_SET_H
#define LLVM_LIBC_HDR_TYPES_FD_SET_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/fd_set.h"

#else // Overlay mode

#include <sys/select.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_FD_SET_H
