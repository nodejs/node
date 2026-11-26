//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of ns_class type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_NS_CLASS_H
#define LLVM_LIBC_TYPES_NS_CLASS_H

// DNS CLASS and QCLASS values (RFC 1035, RFC 2136).
typedef enum __ns_class {
  ns_c_invalid = 0,
  ns_c_in = 1,     // Internet (IN).
  ns_c_2 = 2,      // Unassigned (formerly CSNET).
  ns_c_chaos = 3,  // Chaos (CH).
  ns_c_hs = 4,     // Hesiod (HS).
  ns_c_none = 254, // QCLASS NONE (RFC 2136).
  ns_c_any = 255,  // QCLASS * (ANY).
  ns_c_max = 65536
} ns_class;

#endif // LLVM_LIBC_TYPES_NS_CLASS_H
