//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct __pthread_cleanup_frame.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT___PTHREAD_CLEANUP_FRAME_H
#define LLVM_LIBC_TYPES_STRUCT___PTHREAD_CLEANUP_FRAME_H

struct __pthread_cleanup_frame {
  void (*__routine)(void *);
  void *__arg;
  struct __pthread_cleanup_frame *__next;
};

#endif // LLVM_LIBC_TYPES_STRUCT___PTHREAD_CLEANUP_FRAME_H
