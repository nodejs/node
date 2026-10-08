//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy for struct sigevent.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_STRUCT_SIGEVENT_H
#define LLVM_LIBC_HDR_TYPES_STRUCT_SIGEVENT_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/struct_sigevent.h"

#else

#include <signal.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_STRUCT_SIGEVENT_H
