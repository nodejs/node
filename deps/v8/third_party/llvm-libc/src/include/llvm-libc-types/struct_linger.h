//===-- Definition of struct linger ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_LINGER_H
#define LLVM_LIBC_TYPES_STRUCT_LINGER_H

struct linger {
  int l_onoff;  // Nonzero means "on"
  int l_linger; // Number of seconds to linger
};

#endif // LLVM_LIBC_TYPES_STRUCT_LINGER_H
