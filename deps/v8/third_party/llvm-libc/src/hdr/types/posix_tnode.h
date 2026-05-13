//===-- Definition of macros from posix_tnode -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_POSIX_TNODE_H
#define LLVM_LIBC_HDR_TYPES_POSIX_TNODE_H

// posix_tnode is introduced in POSIX.1-2024.
// this may result in problems when overlaying on older systems.
// internal code should use __llvm_libc_tnode.

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/posix_tnode.h"
#define __llvm_libc_tnode posix_tnode

#else // Overlay mode

#include <search.h>
typedef void __llvm_libc_tnode;

#endif // LLVM_LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_POSIX_TNODE_H
