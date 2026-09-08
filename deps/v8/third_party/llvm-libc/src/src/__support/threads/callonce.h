//===-- Types related to the callonce function ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_CALLONCE_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_CALLONCE_H

#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_LIKELY

// Plaform specific routines, provides:
// - OnceFlag definition
// - callonce_impl::callonce_fastpath for fast path check
// - callonce_impl::callonce_slowpath for slow path execution
#ifdef __linux__
#include "src/__support/threads/linux/callonce.h"
#else
#error "callonce is not supported on this platform"
#endif

namespace LIBC_NAMESPACE_DECL {
template <class CallOnceCallback>
LIBC_INLINE int callonce(CallOnceFlag *flag, CallOnceCallback callback) {
  if (LIBC_LIKELY(callonce_impl::callonce_fastpath(flag)))
    return 0;

  return callonce_impl::callonce_slowpath(flag, callback);
}
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_CALLONCE_H
