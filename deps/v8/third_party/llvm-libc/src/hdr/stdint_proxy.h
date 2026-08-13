//===-- stdint.h ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_STDINT_PROXY_H
#define LLVM_LIBC_HDR_STDINT_PROXY_H

// This target is to make sure we have correct build order in full build mode,
// that is `libc.include.stdint` is added to the dependency of all targets
// that use <stdint.h> header.

#include <stdint.h>

#endif // LLVM_LIBC_HDR_STDINT_PROXY_H
