//===-- Implementation header for libc_errno --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_LIBC_ERRNO_H
#define LLVM_LIBC_SRC___SUPPORT_LIBC_ERRNO_H

// This header is to be consumed by internal implementations, in which all of
// them should refer to `libc_errno` instead of using `errno` directly from
// <errno.h> header.

// Unit and hermetic tests should:
// - #include "src/__support/libc_errno.h"
// - NOT #include <errno.h>
// - Only use `libc_errno` in the code
// - Depend on libc.src.errno.errno

// Integration tests should:
// - NOT #include "src/__support/libc_errno.h"
// - #include <errno.h>
// - Use regular `errno` in the code
// - Still depend on libc.src.errno.errno

// libc uses a fallback default value, either system or thread local.
#define LIBC_ERRNO_MODE_DEFAULT 0
// libc never stores a value; `errno` macro uses get link-time failure.
#define LIBC_ERRNO_MODE_UNDEFINED 1
// libc maintains per-thread state (requires C++ `thread_local` support).
#define LIBC_ERRNO_MODE_THREAD_LOCAL 2
// libc maintains shared state used by all threads, contrary to standard C
// semantics unless always single-threaded; nothing prevents data races.
#define LIBC_ERRNO_MODE_SHARED 3
// libc doesn't maintain any internal state, instead the embedder must define
// `int *__llvm_libc_errno(void);` C function.
#define LIBC_ERRNO_MODE_EXTERNAL 4
// DEPRECATED: #define LIBC_ERRNO_MODE_SYSTEM 5
// In this mode, the libc_errno is simply a macro resolved to `errno` from the
// system header <errno.h>.  There is no need to link against the
// `libc.src.errno.errno` object, and public C++ symbol
// `LIBC_NAMESPACE::libc_errno` doesn't exist.
#define LIBC_ERRNO_MODE_SYSTEM_INLINE 6

#if !defined(LIBC_ERRNO_MODE) || LIBC_ERRNO_MODE == LIBC_ERRNO_MODE_DEFAULT
#undef LIBC_ERRNO_MODE
#if defined(LIBC_FULL_BUILD) || !defined(LIBC_COPT_PUBLIC_PACKAGING)
#define LIBC_ERRNO_MODE LIBC_ERRNO_MODE_THREAD_LOCAL
#else
#define LIBC_ERRNO_MODE LIBC_ERRNO_MODE_SYSTEM_INLINE
#endif
#endif // LIBC_ERRNO_MODE

#if LIBC_ERRNO_MODE != LIBC_ERRNO_MODE_DEFAULT &&                              \
    LIBC_ERRNO_MODE != LIBC_ERRNO_MODE_UNDEFINED &&                            \
    LIBC_ERRNO_MODE != LIBC_ERRNO_MODE_THREAD_LOCAL &&                         \
    LIBC_ERRNO_MODE != LIBC_ERRNO_MODE_SHARED &&                               \
    LIBC_ERRNO_MODE != LIBC_ERRNO_MODE_EXTERNAL &&                             \
    LIBC_ERRNO_MODE != LIBC_ERRNO_MODE_SYSTEM_INLINE
#error LIBC_ERRNO_MODE must be one of the following values: \
LIBC_ERRNO_MODE_DEFAULT, \
LIBC_ERRNO_MODE_UNDEFINED, \
LIBC_ERRNO_MODE_THREAD_LOCAL, \
LIBC_ERRNO_MODE_SHARED, \
LIBC_ERRNO_MODE_EXTERNAL, \
LIBC_ERRNO_MODE_SYSTEM_INLINE.
#endif

#if LIBC_ERRNO_MODE == LIBC_ERRNO_MODE_SYSTEM_INLINE

#include <errno.h>

#define libc_errno errno

#else // !LIBC_ERRNO_MODE_SYSTEM_INLINE

#include "hdr/errno_macros.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

extern "C" int *__llvm_libc_errno() noexcept;

struct Errno {
  void operator=(int);
  operator int();
};

extern Errno libc_errno;

} // namespace LIBC_NAMESPACE_DECL

using LIBC_NAMESPACE::libc_errno;

#endif // LIBC_ERRNO_MODE_SYSTEM_INLINE

#endif // LLVM_LIBC_SRC___SUPPORT_LIBC_ERRNO_H
