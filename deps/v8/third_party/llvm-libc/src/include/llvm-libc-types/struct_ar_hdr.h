//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ar_hdr.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_AR_HDR_H
#define LLVM_LIBC_TYPES_STRUCT_AR_HDR_H

// Archive member header. Every field is ASCII text padded with spaces.
// Format reference: https://man.freebsd.org/cgi/man.cgi?query=ar&sektion=5
struct ar_hdr {
  char ar_name[16]; // Member name, terminated by '/' in the System V format.
  char ar_date[12]; // Modification time, decimal seconds since the Epoch.
  char ar_uid[6];   // User ID, decimal.
  char ar_gid[6];   // Group ID, decimal.
  char ar_mode[8];  // File mode, octal.
  char ar_size[10]; // Member size in bytes, decimal.
  char ar_fmag[2];  // Always ARFMAG.
};

#endif // LLVM_LIBC_TYPES_STRUCT_AR_HDR_H
