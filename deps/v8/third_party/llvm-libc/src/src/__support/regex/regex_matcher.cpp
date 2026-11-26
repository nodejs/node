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

#include "hdr/regex_macros.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_expr_pool.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

namespace {

// Computes the active AnchorKind bitmask at fencepost position pos in input.
uint8_t ctx_at(cpp::string_view input, size_t pos, int cflags, int eflags) {
  LIBC_ASSERT(pos <= input.size());
  uint8_t ctx = 0;
  if ((pos == 0 && !(eflags & REG_NOTBOL)) ||
      (pos > 0 && (cflags & REG_NEWLINE) && input[pos - 1] == '\n'))
    ctx |= ANCHOR_BOL;
  if ((pos == input.size() && !(eflags & REG_NOTEOL)) ||
      (pos < input.size() && (cflags & REG_NEWLINE) && input[pos] == '\n'))
    ctx |= ANCHOR_EOL;
  return ctx;
}

} // namespace

ErrorOr<ExprId> derivative(ExprId id, char c, ExprPool &pool,
                           uint8_t ctx_before) {
  if (!pool.is_valid_input(id))
    return Error(REG_BADPAT);
  const Expr e = pool.get(id);
  switch (e.kind) {
  case ExprKind::EmptySet:
  case ExprKind::EmptyStr:
  case ExprKind::Anchor:
    return EMPTY_SET_ID;
  case ExprKind::Literal:
    return (c == e.ch) ? EMPTY_STR_ID : EMPTY_SET_ID;
  case ExprKind::ByteSet:
    return pool.get_byte_set(e.byte_set_id).test(static_cast<uint8_t>(c))
               ? EMPTY_STR_ID
               : EMPTY_SET_ID;
  case ExprKind::Concat: {
    // Product Rule:
    // D_c(L . R) = (D_c(L) . R) | (nullable(L, ctx_before) ? D_c(R) : EmptySet)
    const bool left_nullable = nullable(e.bin.left, ctx_before, pool);
    ErrorOr<ExprId> d_l = derivative(e.bin.left, c, pool, ctx_before);
    if (!d_l.has_value())
      return d_l;
    ErrorOr<ExprId> lhs = pool.make_concat(*d_l, e.bin.right);
    if (!lhs.has_value() || !left_nullable)
      return lhs;
    ErrorOr<ExprId> d_r = derivative(e.bin.right, c, pool, ctx_before);
    if (!d_r.has_value())
      return d_r;
    return pool.make_alt(*lhs, *d_r);
  }
  case ExprKind::Alt: {
    // Sum Rule: D_c(L | R) = D_c(L) | D_c(R)
    ErrorOr<ExprId> d_l = derivative(e.bin.left, c, pool, ctx_before);
    if (!d_l.has_value())
      return d_l;
    ErrorOr<ExprId> d_r = derivative(e.bin.right, c, pool, ctx_before);
    if (!d_r.has_value())
      return d_r;
    return pool.make_alt(*d_l, *d_r);
  }
  case ExprKind::Star: {
    // Kleene Rule: D_c(R*) = D_c(R) . R*
    ErrorOr<ExprId> d_body = derivative(e.body, c, pool, ctx_before);
    if (!d_body.has_value())
      return d_body;
    return pool.make_concat(*d_body, id);
  }
  case ExprKind::Repeat: {
    // Bounded Repetition Rule:
    // D_c(R{m,n}) = D_c(R) . R{nullable(R, ctx_before) ? 0 : max(0,m-1), n-1}.
    // When R is nullable at ctx_before (e.g. via '^'), up to m - 1 leading
    // copies may match empty at ctx_before before the copy that consumes c.
    ErrorOr<ExprId> d_body = derivative(e.rep.body, c, pool, ctx_before);
    if (!d_body.has_value() || *d_body == EMPTY_SET_ID)
      return d_body;
    uint16_t next_min =
        (e.rep.min > 1 && !nullable(e.rep.body, ctx_before, pool))
            ? static_cast<uint16_t>(e.rep.min - 1)
            : static_cast<uint16_t>(0);
    uint16_t next_max = (e.rep.max == REPEAT_INF)
                            ? REPEAT_INF
                            : static_cast<uint16_t>(e.rep.max - 1);
    ErrorOr<ExprId> tail = pool.make_repeat(e.rep.body, next_min, next_max);
    if (!tail.has_value())
      return tail;
    return pool.make_concat(*d_body, *tail);
  }
  }
  LIBC_ASSERT(false);
  __builtin_unreachable();
}

MatchResult match(ExprPool &pool, const ExprId pattern, cpp::string_view input,
                  int cflags, int eflags) {
  if (!pool.is_valid_input(pattern) || pattern == EMPTY_SET_ID)
    return MatchResult::NoMatch;
  uint8_t ctx = ctx_at(input, 0, cflags, eflags);
  if (nullable(pattern, ctx, pool))
    return MatchResult::Match;

  // Unanchored substring search: at each byte, step active partial matches via
  // derivative(state, c, pool, ctx) and union with pattern so a new match can
  // begin at the next fencepost (equivalent to matching .* . pattern without
  // constructing a prefix wildcard node).
  ExprId state = pattern;
  for (size_t pos = 0; pos < input.size(); ++pos) {
    ErrorOr<ExprId> d = derivative(state, input[pos], pool, ctx);
    if (!d.has_value())
      return MatchResult::OutOfMemory;
    ctx = ctx_at(input, pos + 1, cflags, eflags);
    if (nullable(*d, ctx, pool) || nullable(pattern, ctx, pool))
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
