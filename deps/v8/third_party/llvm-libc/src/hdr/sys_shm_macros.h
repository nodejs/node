//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of macros from sys/shm.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_SYS_SHM_MACROS_H
#define LLVM_LIBC_HDR_SYS_SHM_MACROS_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-macros/sys-shm-macros.h"

#else // Overlay mode

#include <stdint.h>
#include <sys/shm.h>

// Older system sys/shm.h headers may not define POSIX.1-2024 SHM_FAILED yet.
#ifndef SHM_FAILED
#define SHM_FAILED (reinterpret_cast<void *>(static_cast<intptr_t>(-1)))
#endif // SHM_FAILED

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_SYS_SHM_MACROS_H
