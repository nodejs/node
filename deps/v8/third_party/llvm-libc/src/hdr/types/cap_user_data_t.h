//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy header for cap_user_data_t.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_CAP_USER_DATA_T_H
#define LLVM_LIBC_HDR_TYPES_CAP_USER_DATA_T_H

#ifdef __linux__
#include <linux/capability.h>
#endif // __linux__

#endif // LLVM_LIBC_HDR_TYPES_CAP_USER_DATA_T_H
