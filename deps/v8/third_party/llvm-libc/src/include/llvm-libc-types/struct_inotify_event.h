//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct inotify_event.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_INOTIFY_EVENT_H
#define LLVM_LIBC_TYPES_STRUCT_INOTIFY_EVENT_H

#include "../llvm-libc-macros/stdint-macros.h"

struct inotify_event {
  int wd;
  uint32_t mask;
  uint32_t cookie;
  uint32_t len;
  char name[];
};

#endif // LLVM_LIBC_TYPES_STRUCT_INOTIFY_EVENT_H
