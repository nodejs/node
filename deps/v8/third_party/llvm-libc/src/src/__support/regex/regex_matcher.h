//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Brzozowski derivative evaluation and regular expression matching interface.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_MATCHER_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_MATCHER_H

#include "src/__support/CPP/string_view.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_expr_pool.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

/// Outcome of evaluating a regular expression against an input string.
enum class MatchResult {
  Match,
  NoMatch,
  OutOfMemory,
};

/// Computes the Brzozowski derivative D_c(id) with respect to character c.
///
/// Formally, D_c(L) = { w | c . w in L }.
ErrorOr<ExprId> derivative(ExprId id, char c, ExprPool &pool);

/// Evaluates whether pattern matches any substring of input using lazy
/// Brzozowski derivatives.
MatchResult match(ExprPool &pool, const ExprId pattern, cpp::string_view input);

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_MATCHER_H
