//===-- String to float conversion utils ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_STR_TO_FLOAT_H
#define LLVM_LIBC_SHARED_STR_TO_FLOAT_H

#include "libc_common.h"
#include "src/__support/str_to_float.h"

namespace LIBC_NAMESPACE_DECL {
namespace shared {

using internal::ExpandedFloat;
using internal::FloatConvertReturn;
using internal::RoundDirection;

using internal::binary_exp_to_float;
using internal::decimal_exp_to_float;

} // namespace shared
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SHARED_STR_TO_FLOAT_H
