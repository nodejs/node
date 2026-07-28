//===-- Definition of type struct sched_param -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SCHED_PARAM_H
#define LLVM_LIBC_TYPES_STRUCT_SCHED_PARAM_H

#include "pid_t.h"
#include "struct_timespec.h"
#include "time_t.h"

struct sched_param {
  // Process or thread execution scheduling priority.
  int sched_priority;
};

#endif // LLVM_LIBC_TYPES_STRUCT_SCHED_PARAM_H
