//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct group_req.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_GROUP_REQ_H
#define LLVM_LIBC_TYPES_STRUCT_GROUP_REQ_H

#include "../llvm-libc-macros/stdint-macros.h"
#include "struct_sockaddr_storage.h"

struct group_req {
  uint32_t gr_interface;
  // NB: Architecture-specific padding.
  struct sockaddr_storage gr_group;
};

#endif // LLVM_LIBC_TYPES_STRUCT_GROUP_REQ_H
