//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy header for eventfd_t.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_EVENTFD_T_H
#define LLVM_LIBC_HDR_TYPES_EVENTFD_T_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/eventfd_t.h"

#else // Overlay mode

#include <sys/eventfd.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_EVENTFD_T_H
