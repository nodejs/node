//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Macros defined in arpa/nameser.h header file.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_ARPA_NAMESER_MACROS_H
#define LLVM_LIBC_MACROS_ARPA_NAMESER_MACROS_H

// DNS constants based on RFC 1035.
#define NS_PACKETSZ 512   // Maximum UDP message size (without EDNS).
#define NS_MAXDNAME 1025  // Buffer size for a domain name in text form.
#define NS_MAXMSG 65535   // Maximum message size.
#define NS_MAXCDNAME 255  // Maximum domain name length in wire format.
#define NS_MAXLABEL 63    // Maximum label length.
#define NS_HFIXEDSZ 12    // Size of the message header.
#define NS_QFIXEDSZ 4     // Size of the fixed part of a question.
#define NS_RRFIXEDSZ 10   // Size of the fixed part of a resource record.
#define NS_INT32SZ 4      // Size of a 32-bit integer.
#define NS_INT16SZ 2      // Size of a 16-bit integer.
#define NS_INT8SZ 1       // Size of an 8-bit integer.
#define NS_INADDRSZ 4     // Size of an IPv4 address.
#define NS_IN6ADDRSZ 16   // Size of an IPv6 address.
#define NS_CMPRSFLGS 0xc0 // Flag bits marking a compressed name pointer.
#define NS_DEFAULTPORT 53 // Server port for both UDP and TCP.

#endif // LLVM_LIBC_MACROS_ARPA_NAMESER_MACROS_H
