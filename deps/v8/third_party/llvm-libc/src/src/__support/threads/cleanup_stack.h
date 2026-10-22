//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Management of thread-specific cleanup handlers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_CLEANUP_STACK_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_CLEANUP_STACK_H

#include "hdr/types/struct___pthread_cleanup_frame.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

class CleanupStack {
public:
  LIBC_INLINE void push(__pthread_cleanup_frame *frame) {
    frame->__next = top;
    top = frame;
  }

  LIBC_INLINE __pthread_cleanup_frame *pop() {
    __pthread_cleanup_frame *frame = top;
    if (frame) {
      top = frame->__next;
      frame->__next = nullptr;
    }
    return frame;
  }

private:
  __pthread_cleanup_frame *top = nullptr;
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_CLEANUP_STACK_H
