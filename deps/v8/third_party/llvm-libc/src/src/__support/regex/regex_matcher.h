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
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_expr_pool.h"
#include "src/__support/regex/regex_internal.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

/// Outcome of evaluating a regular expression against an input string.
enum class MatchResult {
  Match,
  NoMatch,
  OutOfMemory,
};

/// Evaluates whether id can match the empty string at a fencepost with the
/// given active AnchorKind bitmask.
bool nullable(ExprId id, uint8_t anchor_ctx, const ExprPool &pool);

/// Computes the Brzozowski derivative D_c(id) with respect to character c
/// when stepping across c from a fencepost with active anchor mask ctx_before.
///
/// Formally, D_c(L) = { w | c . w in L }.
ErrorOr<ExprId> derivative(ExprId id, char c, ExprPool &pool,
                           uint8_t ctx_before = 0);

/// Evaluates whether pattern matches any substring of input using lazy
/// Brzozowski derivatives.
MatchResult match(ExprPool &pool, const ExprId pattern, cpp::string_view input,
                  int cflags = 0, int eflags = 0);

/// Evaluates a compiled RegexInternal against input.
LIBC_INLINE MatchResult match(RegexInternal &internal, cpp::string_view input,
                              int eflags = 0) {
  return match(internal.pool, internal.root, input, internal.cflags, eflags);
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_MATCHER_H
