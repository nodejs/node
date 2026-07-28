//===-- Definition of type posix_spawn_file_actions_t ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_POSIX_SPAWN_FILE_ACTIONS_T_H
#define LLVM_LIBC_TYPES_POSIX_SPAWN_FILE_ACTIONS_T_H

typedef struct {
  void *__front;
  void *__back;
} posix_spawn_file_actions_t;

#endif // LLVM_LIBC_TYPES_POSIX_SPAWN_FILE_ACTIONS_T_H
