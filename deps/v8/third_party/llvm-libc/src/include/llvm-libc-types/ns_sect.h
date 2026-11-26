//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of ns_sect type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_NS_SECT_H
#define LLVM_LIBC_TYPES_NS_SECT_H

// DNS message section indices (RFC 1035, RFC 2136).
typedef enum __ns_sect {
  ns_s_qd = 0, // Query: Question section.
  ns_s_zn = 0, // Update: Zone section.
  ns_s_an = 1, // Query: Answer section.
  ns_s_pr = 1, // Update: Prerequisite section.
  ns_s_ns = 2, // Query: Authority section.
  ns_s_ud = 2, // Update: Update section.
  ns_s_ar = 3, // Additional records section.
  ns_s_max = 4
} ns_sect;

#endif // LLVM_LIBC_TYPES_NS_SECT_H
