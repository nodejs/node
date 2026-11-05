//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Macros defined in net/if.h header file.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_NET_IF_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_NET_IF_MACROS_H

// Prevent the linux headers from defining this macro.
#define __UAPI_DEF_IF_IFNAMSIZ 0

#define IF_NAMESIZE 16
#define IFNAMSIZ IF_NAMESIZE

#endif // LLVM_LIBC_MACROS_LINUX_NET_IF_MACROS_H
