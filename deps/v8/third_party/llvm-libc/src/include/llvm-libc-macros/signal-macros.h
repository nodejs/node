//===-- Definition of signal number macros --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_SIGNAL_MACROS_H
#define LLVM_LIBC_MACROS_SIGNAL_MACROS_H

#if defined(__linux__)
#include "linux/signal-macros.h"
#elif defined(__NVPTX__) || defined(__AMDGPU__) || defined(__SPIRV__)
#include "gpu/signal-macros.h"
#endif

#endif // LLVM_LIBC_MACROS_SIGNAL_MACROS_H
