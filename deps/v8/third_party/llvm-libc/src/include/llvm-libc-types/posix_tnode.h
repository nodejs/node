//===-- Definition of type posix_tnode--------------- ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_POSIX_TNODE_H
#define LLVM_LIBC_TYPES_POSIX_TNODE_H

// Following POSIX.1-2024 and Austin Group Defect Report 1011:
// https://austingroupbugs.net/view.php?id=1011
// Define posix_tnode as void to provide both type safety and compatibility.
typedef void posix_tnode;

#endif // LLVM_LIBC_TYPES_POSIX_TNODE_H
