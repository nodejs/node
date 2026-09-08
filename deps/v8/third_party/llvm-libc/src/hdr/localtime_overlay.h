//===-- Including localtime.h in overlay mode -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_LOCALTIME_OVERLAY_H
#define LLVM_LIBC_HDR_LOCALTIME_OVERLAY_H

#ifdef LIBC_FULL_BUILD
#error "This header should only be included in overlay mode"
#endif

// Overlay mode

// glibc <unistd.h> header might provide extern inline definitions for few
// functions, causing external alias errors.  They are guarded by
// `__USE_EXTERN_INLINES` macro.

#ifdef __USE_EXTERN_INLINES
#define LIBC_OLD_USE_EXTERN_INLINES
#undef __USE_EXTERN_INLINES
#endif

#ifndef __NO_INLINE__
#define __NO_INLINE__ 1
#define LIBC_SET_NO_INLINE
#endif

#include <localtime.h>
#include <localtime_r.h>

#ifdef LIBC_SET_NO_INLINE
#undef __NO_INLINE__
#undef LIBC_SET_NO_INLINE
#endif

#ifdef LIBC_OLD_USE_EXTERN_INLINES
#define __USE_EXTERN_INLINES
#undef LIBC_OLD_USE_EXTERN_INLINES
#endif

#endif // LLVM_LIBC_HDR_LOCALTIME_OVERLAY_H
