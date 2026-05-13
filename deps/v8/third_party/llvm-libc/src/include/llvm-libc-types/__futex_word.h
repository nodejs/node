//===-- Definition of type which can represent a futex word ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES___FUTEX_WORD_H
#define LLVM_LIBC_TYPES___FUTEX_WORD_H

#include "__llvm-libc-common.h"

typedef struct {
  // Futex word should be aligned appropriately to allow target atomic
  // instructions. This declaration mimics the internal setup.
  _Alignas(sizeof(__UINT32_TYPE__) > _Alignof(__UINT32_TYPE__)
               ? sizeof(__UINT32_TYPE__)
               : _Alignof(__UINT32_TYPE__)) __UINT32_TYPE__ __word;
} __futex_word;

#endif // LLVM_LIBC_TYPES___FUTEX_WORD_H
