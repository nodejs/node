//===-- Definition of macros from sys/time.h ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_TIME_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_TIME_MACROS_H

// Add two timevals and put the result in timeval_ptr_result. If the resulting
// usec value is greater than 999,999 then the microseconds are turned into full
// seconds (1,000,000 is subtracted from usec and 1 is added to sec).
#define timeradd(timeval_ptr_a, timeval_ptr_b, timeval_ptr_result)             \
  (timeval_ptr_result)->tv_sec =                                               \
      (timeval_ptr_a)->tv_sec + (timeval_ptr_b)->tv_sec +                      \
      (((timeval_ptr_a)->tv_usec + (timeval_ptr_b)->tv_usec) >= 1000000 ? 1    \
                                                                        : 0);  \
  (timeval_ptr_result)->tv_usec =                                              \
      (timeval_ptr_a)->tv_usec + (timeval_ptr_b)->tv_usec -                    \
      (((timeval_ptr_a)->tv_usec + (timeval_ptr_b)->tv_usec) >= 1000000        \
           ? 1000000                                                           \
           : 0);

// Subtract two timevals and put the result in timeval_ptr_result. If the
// resulting usec value is less than 0 then 1,000,000 is added to usec and 1 is
// subtracted from sec.
#define timersub(timeval_ptr_a, timeval_ptr_b, timeval_ptr_result)             \
  (timeval_ptr_result)->tv_sec =                                               \
      (timeval_ptr_a)->tv_sec - (timeval_ptr_b)->tv_sec -                      \
      (((timeval_ptr_a)->tv_usec - (timeval_ptr_b)->tv_usec) < 0 ? 1 : 0);     \
  (timeval_ptr_result)->tv_usec =                                              \
      (timeval_ptr_a)->tv_usec - (timeval_ptr_b)->tv_usec +                    \
      (((timeval_ptr_a)->tv_usec - (timeval_ptr_b)->tv_usec) < 0 ? 1000000     \
                                                                 : 0);

// Reset a timeval to the epoch.
#define timerclear(timeval_ptr)                                                \
  (timeval_ptr)->tv_sec = 0;                                                   \
  (timeval_ptr)->tv_usec = 0;

// Determine if a timeval is set to the epoch.
#define timerisset(timeval_ptr)                                                \
  (timeval_ptr)->tv_sec != 0 || (timeval_ptr)->tv_usec != 0;

// Compare two timevals using CMP.
#define timercmp(timeval_ptr_a, timeval_ptr_b, CMP)                            \
  (((timeval_ptr_a)->tv_sec == (timeval_ptr_b)->tv_sec)                        \
       ? ((timeval_ptr_a)->tv_usec CMP(timeval_ptr_b)->tv_usec)                \
       : ((timeval_ptr_a)->tv_sec CMP(timeval_ptr_b)->tv_sec))

#endif // LLVM_LIBC_MACROS_LINUX_SYS_TIME_MACROS_H
