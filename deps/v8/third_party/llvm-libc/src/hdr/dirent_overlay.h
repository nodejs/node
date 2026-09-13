//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Overlay header for dirent.h in overlay build mode.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_DIRENT_OVERLAY_H
#define LLVM_LIBC_HDR_DIRENT_OVERLAY_H

#ifdef LIBC_FULL_BUILD
#error "This header should only be included in overlay mode"
#endif

#include <dirent.h>

#endif // LLVM_LIBC_HDR_DIRENT_OVERLAY_H
