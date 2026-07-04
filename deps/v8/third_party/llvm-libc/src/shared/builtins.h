//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header aggregates LLVM-libc's shared compiler-rt builtins so that
/// they can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_BUILTINS_H
#define LLVM_LIBC_SHARED_BUILTINS_H

#include "libc_common.h"

#include "builtins/adddf3.h"
#include "builtins/addsf3.h"
#include "builtins/addtf3.h"
#include "builtins/divdf3.h"
#include "builtins/divtf3.h"
#include "builtins/muldf3.h"
#include "builtins/multf3.h"
#include "builtins/subdf3.h"
#include "builtins/subtf3.h"

#endif // LLVM_LIBC_SHARED_BUILTINS_H
