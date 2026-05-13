//===-- Map of Linux extension signal numbers to strings --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_STRINGUTIL_TABLES_LINUX_EXTENSION_SIGNALS_H
#define LLVM_LIBC_SRC___SUPPORT_STRINGUTIL_TABLES_LINUX_EXTENSION_SIGNALS_H

#include "src/__support/StringUtil/message_mapper.h"
#include "src/__support/macros/config.h"

#include <signal.h> // For signal numbers

namespace LIBC_NAMESPACE_DECL {

// The array being larger than necessary isn't a problem. The MsgMappings will
// be set to their default state which maps 0 to an empty string. This will get
// filtered out in the MessageMapper building stage.
LIBC_INLINE_VAR constexpr const MsgTable<3> LINUX_SIGNALS = {
#ifdef SIGSTKFLT
    MsgMapping(SIGSTKFLT, "Stack fault"), // unused
#endif
    MsgMapping(SIGWINCH, "Window changed"),
#ifdef SIGPWR
    MsgMapping(SIGPWR, "Power failure"), // ignored
#endif
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_STRINGUTIL_TABLES_LINUX_EXTENSION_SIGNALS_H
