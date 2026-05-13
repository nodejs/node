//===-- Definition of struct sembuf ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SEMBUF_H
#define LLVM_LIBC_TYPES_STRUCT_SEMBUF_H

struct sembuf {
  // changed to unsigned short from short since POSIX issue 7
  unsigned short sem_num;
  short sem_op;
  short sem_flg;
};

#endif // LLVM_LIBC_TYPES_STRUCT_SEMBUF_H
