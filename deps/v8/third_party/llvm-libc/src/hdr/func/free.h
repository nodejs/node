//===-- Definition of the free.h proxy ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_FUNC_FREE_H
#define LLVM_LIBC_HDR_FUNC_FREE_H

#ifdef LIBC_FULL_BUILD

#include "include/__llvm-libc-common.h"

__BEGIN_C_DECLS
void free(void *) __NOEXCEPT;
__END_C_DECLS

#else // Overlay mode

#include "hdr/stdlib_overlay.h"

#endif

#endif
