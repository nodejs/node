//===-- Shared sign type ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_SIGN_H
#define LLVM_LIBC_SHARED_SIGN_H

#include "libc_common.h"
#include "src/__support/sign.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {

using LIBC_NAMESPACE::Sign;

} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SHARED_SIGN_H
