//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal path helpers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_PATH_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_PATH_H

#if defined(__linux__)
#include "src/__support/OSUtil/linux/path.h"
#else
#error path.h unavailable on this platfrom
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_PATH_H
