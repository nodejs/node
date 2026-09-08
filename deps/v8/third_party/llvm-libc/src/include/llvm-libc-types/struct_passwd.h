//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct passwd.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_PASSWD_H
#define LLVM_LIBC_TYPES_STRUCT_PASSWD_H

#include "gid_t.h"
#include "uid_t.h"

/// Structure representing user account information from the password database.
struct passwd {
  char *pw_name;   // User's login name.
  char *pw_passwd; // Encrypted password.
  uid_t pw_uid;    // Numerical user ID.
  gid_t pw_gid;    // Numerical group ID.
  char *pw_gecos;  // User info / real name.
  char *pw_dir;    // Initial working directory.
  char *pw_shell;  // Program to use as shell.
};

#endif // LLVM_LIBC_TYPES_STRUCT_PASSWD_H
