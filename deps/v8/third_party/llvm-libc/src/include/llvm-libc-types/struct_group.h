//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct group.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_GROUP_H
#define LLVM_LIBC_TYPES_STRUCT_GROUP_H

#include "gid_t.h"

// Structure representing group information in the group database.
struct group {
  char *gr_name;   // The name of the group.
  char *gr_passwd; // Group password.
  gid_t gr_gid;    // Numerical group ID.
  char **gr_mem;   // Pointer to a null-terminated array of member names.
};

#endif // LLVM_LIBC_TYPES_STRUCT_GROUP_H
