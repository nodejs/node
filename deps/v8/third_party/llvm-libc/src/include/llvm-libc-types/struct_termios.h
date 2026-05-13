//===-- Definition of struct termios --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef __LLVM_LIBC_TYPES_STRUCT_TERMIOS_H__
#define __LLVM_LIBC_TYPES_STRUCT_TERMIOS_H__

#include "cc_t.h"
#include "speed_t.h"
#include "tcflag_t.h"

struct termios {
  tcflag_t c_iflag; // Input mode flags
  tcflag_t c_oflag; // Output mode flags
  tcflag_t c_cflag; // Control mode flags
  tcflag_t c_lflag; // Local mode flags
#ifdef __linux__
  cc_t c_line; // Line discipline
#endif         // __linux__
  // NCCS is defined in llvm-libc-macros/termios-macros.h.
  cc_t c_cc[NCCS]; // Control characters
#ifdef __linux__
  speed_t c_ispeed; // Input speed
  speed_t c_ospeed; // output speed
#endif              // __linux__
};

#endif // __LLVM_LIBC_TYPES_STRUCT_TERMIOS_H__
