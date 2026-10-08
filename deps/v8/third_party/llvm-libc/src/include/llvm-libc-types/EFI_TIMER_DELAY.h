//===-- Definition of EFI_TIMER_DELAY type --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_TIMER_DELAY_H
#define LLVM_LIBC_TYPES_EFI_TIMER_DELAY_H

typedef enum {
  TimerCancel,
  TimerPeriodic,
  TimerRelative,
} EFI_TIMER_DELAY;

#endif // LLVM_LIBC_TYPES_EFI_TIMER_DELAY_H
