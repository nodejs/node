//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy header for struct __pthread_cleanup_frame.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_STRUCT___PTHREAD_CLEANUP_FRAME_H
#define LLVM_LIBC_HDR_TYPES_STRUCT___PTHREAD_CLEANUP_FRAME_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/struct___pthread_cleanup_frame.h"

#else // Overlay mode

#error "pthread_cleanup functionality not available in overlay mode"

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_STRUCT___PTHREAD_CLEANUP_FRAME_H
