//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file instantiates the functionality needed for supporting floating
/// point arguments in modular printf builds. Non-modular printf builds
/// implicitly instantiate these functions.
///
//===----------------------------------------------------------------------===//

#ifdef LIBC_COPT_PRINTF_MODULAR

#define LIBC_PRINTF_DEFINE_MODULES
#include "src/__support/printf_core/converter.h"

// Bring this file into the link if __printf_float is referenced.
extern "C" void __printf_float() {}

#endif // LIBC_COPT_PRINTF_MODULAR
