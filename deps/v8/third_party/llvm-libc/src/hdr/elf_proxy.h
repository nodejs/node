//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy header for elf.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_HDR_ELF_PROXY_H
#define LLVM_LIBC_HDR_HDR_ELF_PROXY_H

#ifdef LIBC_FULL_BUILD

#include "llvm-libc-proxy/elf_proxy.h"

#else // Overlay mode

#include <elf.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_HDR_ELF_PROXY_H
