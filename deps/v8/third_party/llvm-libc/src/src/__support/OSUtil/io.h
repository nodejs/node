//===---------------- Implementation of IO utils ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_IO_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_IO_H

#include "src/__support/macros/properties/architectures.h"

#if defined(LIBC_TARGET_ARCH_IS_GPU)
#include "gpu/io.h"
#elif defined(__APPLE__)
#include "darwin/io.h"
#elif defined(__linux__)
#include "linux/io.h"
#elif defined(__Fuchsia__)
#include "fuchsia/io.h"
#elif defined(_WIN32)
#include "windows/io.h"
#elif defined(__ELF__)
// TODO: Ideally we would have LIBC_TARGET_OS_IS_BAREMETAL.
#include "baremetal/io.h"
#elif defined(__UEFI__)
#include "uefi/io.h"
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_IO_H
