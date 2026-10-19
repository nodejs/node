//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Brzozowski derivative evaluation and regular expression matching
/// implementation.
///
//===----------------------------------------------------------------------===//

#include "src/__support/regex/regex_matcher.h"

#include "src/__support/CPP/string_view.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_expr_pool.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

ErrorOr<ExprId> derivative(ExprId id, char c, ExprPool &pool) {
  // Copy the 16-byte node by value as recursive calls below mutate pool.
  const Expr e = pool.get(id);
  switch (e.kind) {
  case ExprKind::EmptySet:
  case ExprKind::EmptyStr:
    return EMPTY_SET_ID;
  case ExprKind::Literal:
    return (c == e.ch) ? EMPTY_STR_ID : EMPTY_SET_ID;
  case ExprKind::Concat: {
    // Product Rule:
    // D_c(L . R) = (D_c(L) . R) | (nullable(L) ? D_c(R) : EmptySet)
    const bool left_nullable = pool.is_nullable(e.bin.left);
    ErrorOr<ExprId> d_l = derivative(e.bin.left, c, pool);
    if (!d_l.has_value())
      return d_l;
    ErrorOr<ExprId> lhs = pool.make_concat(*d_l, e.bin.right);
    if (!lhs.has_value() || !left_nullable)
      return lhs;
    ErrorOr<ExprId> d_r = derivative(e.bin.right, c, pool);
    if (!d_r.has_value())
      return d_r;
    return pool.make_alt(*lhs, *d_r);
  }
  case ExprKind::Alt: {
    // Sum Rule: D_c(L | R) = D_c(L) | D_c(R)
    ErrorOr<ExprId> d_l = derivative(e.bin.left, c, pool);
    if (!d_l.has_value())
      return d_l;
    ErrorOr<ExprId> d_r = derivative(e.bin.right, c, pool);
    if (!d_r.has_value())
      return d_r;
    return pool.make_alt(*d_l, *d_r);
  }
  }
  __builtin_unreachable();
}

MatchResult match(ExprPool &pool, const ExprId pattern,
                  cpp::string_view input) {
  if (pool.is_nullable(pattern))
    return MatchResult::Match;

  // Unanchored substring search: at each byte, step active partial matches via
  // derivative(state, c) and union with pattern so a new match can begin after
  // the current byte (equivalent to matching .* . pattern without constructing
  // a prefix wildcard node).
  ExprId state = pattern;
  for (char c : input) {
    ErrorOr<ExprId> d = derivative(state, c, pool);
    if (!d.has_value())
      return MatchResult::OutOfMemory;
    if (pool.is_nullable(*d))
      return MatchResult::Match;
    ErrorOr<ExprId> next = pool.make_alt(pattern, *d);
    if (!next.has_value())
      return MatchResult::OutOfMemory;
    state = *next;
  }

  return MatchResult::NoMatch;
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL
