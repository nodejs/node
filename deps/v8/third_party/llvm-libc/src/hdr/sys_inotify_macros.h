//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy header for macro values defined in sys/inotify.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_SYS_INOTIFY_MACROS_H
#define LLVM_LIBC_HDR_SYS_INOTIFY_MACROS_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-macros/sys-inotify-macros.h"

#else // Overlay mode

#include <sys/inotify.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_SYS_INOTIFY_MACROS_H
