//===-- Definition of macros from sys/ioctl.h -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_IOCTL_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_IOCTL_MACROS_H

// TODO (michaelrj): Finish defining these macros.
// Just defining this macro for the moment since it's all that we need right
// now. The other macros are mostly just constants, but there's some complexity
// around the definitions of macros like _IO, _IOR, _IOW, and _IOWR that I don't
// think is worth digging into right now.
#define TIOCGETD 0x5424
#define FIONREAD 0x541B

#endif // LLVM_LIBC_MACROS_LINUX_SYS_IOCTL_MACROS_H
