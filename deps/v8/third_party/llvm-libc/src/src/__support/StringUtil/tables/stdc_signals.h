//===-- Map of C standard signal numbers to strings -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_STRINGUTIL_TABLES_STDC_SIGNALS_H
#define LLVM_LIBC_SRC___SUPPORT_STRINGUTIL_TABLES_STDC_SIGNALS_H

#include <signal.h> // For signal numbers

#include "src/__support/StringUtil/message_mapper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

LIBC_INLINE_VAR constexpr const MsgTable<6> STDC_SIGNALS = {
    MsgMapping(SIGINT, "Interrupt"),
    MsgMapping(SIGILL, "Illegal instruction"),
    MsgMapping(SIGABRT, "Aborted"),
    MsgMapping(SIGFPE, "Floating point exception"),
    MsgMapping(SIGSEGV, "Segmentation fault"),
    MsgMapping(SIGTERM, "Terminated"),
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_STRINGUTIL_TABLES_STDC_SIGNALS_H
