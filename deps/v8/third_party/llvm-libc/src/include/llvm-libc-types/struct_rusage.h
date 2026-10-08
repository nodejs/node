//===-- Definition of type struct rusage ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_RUSAGE_H
#define LLVM_LIBC_TYPES_STRUCT_RUSAGE_H

#include "struct_timeval.h"

struct rusage {
  struct timeval ru_utime;
  struct timeval ru_stime;
#ifdef __linux__
  // Following fields are linux extensions as expected by the
  // linux syscalls.
  long ru_maxrss;   // Maximum resident set size
  long ru_ixrss;    // Integral shared memory size
  long ru_idrss;    // Integral unshared data size
  long ru_isrss;    // Integral unshared stack size
  long ru_minflt;   // Page reclaims
  long ru_majflt;   // Page faults
  long ru_nswap;    // Swaps
  long ru_inblock;  // Block input operations
  long ru_oublock;  // Block output operations
  long ru_msgsnd;   // Messages sent
  long ru_msgrcv;   // Messages received
  long ru_nsignals; // Signals received
  long ru_nvcsw;    // Voluntary context switches
  long ru_nivcsw;   // Involuntary context switches
#endif
};

#endif // LLVM_LIBC_TYPES_STRUCT_RUSAGE_H
