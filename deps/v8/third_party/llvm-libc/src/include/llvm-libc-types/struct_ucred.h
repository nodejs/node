//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ucred.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_UCRED_H
#define LLVM_LIBC_TYPES_STRUCT_UCRED_H

#include "gid_t.h"
#include "pid_t.h"
#include "uid_t.h"

// Credentials of a peer process connected to a socket.
struct ucred {
  pid_t pid;
  uid_t uid;
  gid_t gid;
};

#endif // LLVM_LIBC_TYPES_STRUCT_UCRED_H
