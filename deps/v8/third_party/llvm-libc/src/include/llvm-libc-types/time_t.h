//===-- Definition of the type time_t, for use during the libc build ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_TIME_T_H
#define LLVM_LIBC_TYPES_TIME_T_H

#ifdef LIBC_TYPES_TIME_T_IS_32_BIT
#include "time_t_32.h"
#else
#include "time_t_64.h"
#endif

#endif // LLVM_LIBC_TYPES_TIME_T_H
