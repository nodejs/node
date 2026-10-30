//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal compiled regular expression representation owned by regex_t.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_INTERNAL_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_INTERNAL_H

#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_expr_pool.h"
#include "src/__support/threads/spin_lock.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

/// Internal state stored in regex_t::__internal after a successful regcomp.
struct RegexInternal {
  /// Arena and hash-consing table for the pattern AST and derivative states.
  ExprPool pool;
  /// Root ExprId of the compiled regular expression.
  ExprId root = INVALID_EXPR_ID;
  /// Compilation flags passed to regcomp.
  int cflags = 0;
  /// Spinlock serialising lazy derivative interning across concurrent regexec
  /// calls sharing the same const regex_t*.
  SpinLock lock{};
};

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_INTERNAL_H
