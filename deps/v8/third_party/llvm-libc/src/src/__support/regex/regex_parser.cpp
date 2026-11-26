//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Regular expression parser implementation.
///
//===----------------------------------------------------------------------===//

#include "src/__support/regex/regex_parser.h"

#include "hdr/limits_macros.h"
#include "hdr/regex_macros.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/unique_ptr.h"
#include "src/__support/checked_ptr.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_charclass.h"
#include "src/__support/regex/regex_expr_pool.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

namespace {

// Returns true for backslash escape sequences that represent backreferences or
// boundary assertions rather than escaped literal characters.
constexpr bool is_unimplemented_ere_escape(char c) {
  switch (c) {
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case 'b':
  case 'B':
  case '<':
  case '>':
  case 'A':
  case 'Z':
  case 'z':
  case '`':
  case '\'':
    return true;
  default:
    return false;
  }
}

// Parses a single collating element [[.c.]] or equivalence class [[=c=]].
constexpr ErrorOr<uint8_t> parse_coll_elem(cpp::string_view &rem) {
  LIBC_ASSERT(rem.starts_with("[.") || rem.starts_with("[="));
  cpp::string_view close_delim = rem.starts_with("[.") ? ".]" : "=]";
  rem.remove_prefix(2);
  // TODO: Adopt cpp::string_view::find once implemented in
  // src/__support/CPP/string_view.h.
  size_t delim_pos = 0;
  while (delim_pos + 2 <= rem.size() &&
         !rem.substr(delim_pos).starts_with(close_delim))
    ++delim_pos;
  if (delim_pos + 2 > rem.size())
    return Error(REG_EBRACK);
  cpp::string_view elem = rem.substr(0, delim_pos);
  rem.remove_prefix(delim_pos + 2);
  if (elem.size() == 1)
    return static_cast<uint8_t>(elem.front());
  // "one" is Henry Spencer's cname.h ASCII collating-element test fixture.
  if (elem == "one")
    return static_cast<uint8_t>('1');
  return Error(REG_ECOLLATE);
}

// Parses a single bracket range endpoint (collating symbol or single byte).
constexpr ErrorOr<uint8_t> parse_bracket_elem(cpp::string_view &rem) {
  LIBC_ASSERT(!rem.empty());
  if (rem.starts_with("[."))
    return parse_coll_elem(rem);
  uint8_t c = static_cast<uint8_t>(rem.front());
  rem.remove_prefix(1);
  return c;
}

// Parses a POSIX bracket expression [...], returning a ByteSet node.
ErrorOr<ExprId> parse_bracket_expr(cpp::string_view &rem, int cflags,
                                   ExprPool &pool) {
  bool negate = false;
  if (rem.starts_with('^')) {
    negate = true;
    rem.remove_prefix(1);
  }

  ByteSet bs;
  bool first = true;
  while (!rem.empty()) {
    if (rem.starts_with(']') && !first) {
      rem.remove_prefix(1);
      if (negate) {
        bs.flip();
        bs.reset(0);
        if (cflags & REG_NEWLINE)
          bs.reset('\n');
      }
      return pool.make_byte_set(bs);
    }
    first = false;

    if (rem.starts_with("[:")) {
      rem.remove_prefix(2);
      // TODO: Adopt cpp::string_view::find once implemented in
      // src/__support/CPP/string_view.h.
      size_t delim_pos = 0;
      while (delim_pos + 2 <= rem.size() &&
             !rem.substr(delim_pos).starts_with(":]"))
        ++delim_pos;
      if (delim_pos + 2 > rem.size())
        return Error(REG_EBRACK);
      cpp::string_view name = rem.substr(0, delim_pos);
      rem.remove_prefix(delim_pos + 2);
      if (name == "<" || name == ">")
        return Error(REG_BADPAT);
      if (!add_posix_class(name, bs))
        return Error(REG_ECTYPE);
      if (rem.starts_with('-') && !rem.starts_with("-]"))
        return Error(REG_ERANGE);
      continue;
    }

    if (rem.starts_with("[=")) {
      ErrorOr<uint8_t> eq = parse_coll_elem(rem);
      if (!eq.has_value())
        return Error(eq.error());
      bs.set(*eq);
      if (rem.starts_with('-') && !rem.starts_with("-]"))
        return Error(REG_ERANGE);
      continue;
    }

    ErrorOr<uint8_t> c1 = parse_bracket_elem(rem);
    if (!c1.has_value())
      return Error(c1.error());

    if (rem.starts_with('-') && !rem.starts_with("-]")) {
      rem.remove_prefix(1);
      if (rem.empty())
        return Error(REG_ERANGE);
      if (rem.starts_with("[:") || rem.starts_with("[="))
        return Error(REG_ERANGE);
      ErrorOr<uint8_t> c2 = parse_bracket_elem(rem);
      if (!c2.has_value())
        return Error(c2.error());
      if (*c1 > *c2)
        return Error(REG_ERANGE);
      bs.set_range(*c1, *c2);
      if (rem.starts_with('-') && !rem.starts_with("-]"))
        return Error(REG_ERANGE);
    } else {
      bs.set(*c1);
    }
  }

  return Error(REG_EBRACK);
}

// Maximum parenthesized group nesting depth to prevent parser stack overflow.
constexpr size_t MAX_GROUP_DEPTH = 128;

// Parses a decimal repetition bound in [0, RE_DUP_MAX] from the prefix of spec.
constexpr cpp::optional<uint16_t> consume_bound(cpp::string_view &spec) {
  LIBC_ASSERT(!spec.empty() && internal::isdigit(spec.front()));
  uint32_t val = 0;
  do {
    val = val * 10 + static_cast<uint32_t>(spec.front() - '0');
    if (val > RE_DUP_MAX)
      return cpp::nullopt;
    spec.remove_prefix(1);
  } while (!spec.empty() && internal::isdigit(spec.front()));
  return static_cast<uint16_t>(val);
}

// Parses a trailing '{m}', '{m,}', or '{m,n}' interval quantifier after '{' has
// been matched with a leading digit.
ErrorOr<ExprId> parse_repeat_bounds(cpp::string_view &rem, ExprId atom,
                                    ExprPool &pool, ExprId &prefix_atom) {
  rem.remove_prefix(1);
  // Scan for '}' before parsing bounds so unclosed "a{1,x" reports REG_EBRACE.
  size_t close_pos = rem.find_first_of('}');
  if (close_pos == cpp::string_view::npos)
    return Error(REG_EBRACE);
  cpp::string_view spec = rem.substr(0, close_pos);
  rem.remove_prefix(close_pos + 1);

  cpp::optional<uint16_t> min_opt = consume_bound(spec);
  if (!min_opt.has_value())
    return Error(REG_BADBR);
  uint16_t min_val = *min_opt;
  uint16_t max_val = min_val;
  if (spec.starts_with(',')) {
    spec.remove_prefix(1);
    if (spec.empty()) {
      max_val = REPEAT_INF;
    } else if (internal::isdigit(spec.front())) {
      cpp::optional<uint16_t> max_opt = consume_bound(spec);
      if (!max_opt.has_value() || !spec.empty() || min_val > *max_opt)
        return Error(REG_BADBR);
      max_val = *max_opt;
    } else {
      return Error(REG_BADBR);
    }
  } else if (!spec.empty()) {
    return Error(REG_BADBR);
  }

  uint16_t bound = (max_val == REPEAT_INF) ? min_val : max_val;
  if (static_cast<uint32_t>(bound) * pool.get(atom).rep_expansion >
      MAX_REPEAT_EXPANSION)
    return Error(REG_ESPACE);
  if (min_val == 1 && max_val == REPEAT_INF) {
    ErrorOr<ExprId> star = pool.make_star(atom);
    if (star.has_value())
      prefix_atom = atom;
    return star;
  }
  return pool.make_repeat(atom, min_val, max_val);
}

struct ParsedAtomStack;

ErrorOr<ExprId> parse_alt(cpp::string_view &rem, int cflags, ExprPool &pool,
                          size_t depth, size_t &re_nsub,
                          ParsedAtomStack &stack);

// Parses a single ERE atom and an optional trailing '*', '+', '?', or '{m,n}'
// quantifier.
ErrorOr<ExprId> parse_quantified_atom(cpp::string_view &rem, int cflags,
                                      ExprPool &pool, size_t depth,
                                      size_t &re_nsub, ParsedAtomStack &stack,
                                      ExprId &prefix_atom) {
  char c = rem.front();
  rem.remove_prefix(1);
  ErrorOr<ExprId> atom = INVALID_EXPR_ID;
  bool can_repeat = true;
  switch (c) {
  case '\\': {
    if (rem.empty())
      return Error(REG_EESCAPE);
    c = rem.front();
    rem.remove_prefix(1);
    switch (c) {
    case 'd':
    case 's':
    case 'w': {
      const ByteSet &bs = (c == 'd')   ? DIGIT_CLASS_SET
                          : (c == 's') ? SPACE_CLASS_SET
                                       : WORD_CLASS_SET;
      atom = pool.make_byte_set(bs);
      break;
    }
    case 'D':
    case 'S':
    case 'W': {
      ByteSet bs = (c == 'D')   ? DIGIT_CLASS_SET
                   : (c == 'S') ? SPACE_CLASS_SET
                                : WORD_CLASS_SET;
      bs.flip();
      bs.reset(0);
      if (cflags & REG_NEWLINE)
        bs.reset('\n');
      atom = pool.make_byte_set(bs);
      break;
    }
    default:
      if (is_unimplemented_ere_escape(c))
        return Error(REG_BADPAT);
      atom = pool.make_lit(c);
      break;
    }
    break;
  }
  case '.': {
    ByteSet bs;
    if (cflags & REG_NEWLINE) {
      bs.set_range(0, '\n' - 1);
      bs.set_range('\n' + 1, BYTE_SET_BITS - 1);
    } else {
      bs.flip();
    }
    atom = pool.make_byte_set(bs);
    break;
  }
  case '^':
    atom = pool.make_anchor(ANCHOR_BOL);
    can_repeat = false;
    break;
  case '$':
    atom = pool.make_anchor(ANCHOR_EOL);
    can_repeat = false;
    break;
  // Valid postfix '*', '+', '?', or '{m,n}' is consumed after a repeatable
  // atom below; reaching a quantifier at the start of an atom means it has no
  // preceding repeatable atom (e.g. "*a", "^*", or doubled "a**").
  case '*':
  case '+':
  case '?':
    return Error(REG_BADRPT);
  case '{':
    if (!rem.empty() && internal::isdigit(rem.front()))
      return Error(REG_BADRPT);
    atom = pool.make_lit('{');
    break;
  case '[':
    atom = parse_bracket_expr(rem, cflags, pool);
    break;
  case '(': {
    if (depth >= MAX_GROUP_DEPTH)
      return Error(REG_ESPACE);
    ++re_nsub;
    ErrorOr<ExprId> inner =
        parse_alt(rem, cflags, pool, depth + 1, re_nsub, stack);
    if (!inner.has_value())
      return inner;
    if (!rem.starts_with(')'))
      return Error(REG_EPAREN);
    rem.remove_prefix(1);
    atom = inner;
    break;
  }
  case ')':
    // At depth 0, unmatched ')' is treated as a literal character per
    // POSIX.1-2024 9.4.3.  At depth > 0, parse_concat stops before ')'.
    atom = pool.make_lit(')');
    break;
  default:
    atom = pool.make_lit(c);
    break;
  }

  if (!atom.has_value() || !can_repeat)
    return atom;
  if (rem.starts_with('*')) {
    rem.remove_prefix(1);
    return pool.make_star(*atom);
  }
  // Push r and Star(r) separately rather than Repeat(r, 1, REPEAT_INF) so
  // right-to-left folding links r and r* directly into the enclosing Concat
  // spine without an extra intermediate Concat(r, r*) node when r is itself a
  // Concat (e.g. (ab)+c folds into a . (b . ((ab)* . c))).
  if (rem.starts_with('+')) {
    rem.remove_prefix(1);
    ErrorOr<ExprId> star = pool.make_star(*atom);
    if (star.has_value())
      prefix_atom = *atom;
    return star;
  }
  if (rem.starts_with('?')) {
    rem.remove_prefix(1);
    return pool.make_alt(EMPTY_STR_ID, *atom);
  }
  if (rem.size() >= 2 && rem[0] == '{' && internal::isdigit(rem[1]))
    return parse_repeat_bounds(rem, *atom, pool, prefix_atom);
  return atom;
}

// Shared stack of parsed ExprId nodes across recursive parse_alt and
// parse_concat frames.  Keeps a 64-element (256-byte) inline buffer on the
// top-level parse_ere frame, spilling to the heap only for larger patterns.
// TODO: Replace ParsedAtomStack with an AllocChecker-backed SmallVector once
// available in src/__support/CPP/.
struct ParsedAtomStack {
  static constexpr size_t INLINE_ATOM_COUNT = 64;
  cpp::array<ExprId, INLINE_ATOM_COUNT> inline_atoms{};
  cpp::unique_ptr<ExprId[]> heap_atoms;
  cpp::span<ExprId> atoms{inline_atoms};
  size_t count = 0;

  ParsedAtomStack() = default;
  // `atoms` may point into `inline_atoms`, so the implicit move operations
  // would leave the destination pointing into the source.
  ParsedAtomStack(ParsedAtomStack &&) = delete;
  ParsedAtomStack &operator=(ParsedAtomStack &&) = delete;

  bool push(ExprId id) {
    if (count >= ExprPool::MAX_NODE_LIMIT)
      return false;
    if (count == atoms.size()) {
      size_t new_cap = atoms.size() * 2;
      auto new_atoms = make_unique_for_overwrite<ExprId[]>(new_cap);
      if (!new_atoms)
        return false;
      for (size_t i = 0; i < count; ++i)
        new_atoms[i] = atoms[i];
      heap_atoms = new_atoms.take_unique();
      atoms = cpp::span<ExprId>(heap_atoms.get(), new_cap);
    }
    atoms[count] = id;
    ++count;
    return true;
  }

  ExprId pop() {
    LIBC_ASSERT(count > 0);
    --count;
    return atoms[count];
  }
};

// Parses a single concatenation branch (sequence of quantified atoms).
ErrorOr<ExprId> parse_concat(cpp::string_view &rem, int cflags, ExprPool &pool,
                             size_t depth, size_t &re_nsub,
                             ParsedAtomStack &stack) {
  // Collect quantified atoms for this branch on the shared stack, then fold
  // right-to-left so non-Concat atoms link in O(1) without re-association.
  size_t atom_count = 0;
  while (!rem.empty() && !rem.starts_with('|') &&
         !(depth > 0 && rem.starts_with(')'))) {
    ExprId prefix_atom = INVALID_EXPR_ID;
    ErrorOr<ExprId> atom = parse_quantified_atom(rem, cflags, pool, depth,
                                                 re_nsub, stack, prefix_atom);
    if (!atom.has_value())
      return atom;
    if (prefix_atom != INVALID_EXPR_ID) {
      if (!stack.push(prefix_atom))
        return Error(REG_ESPACE);
      ++atom_count;
    }
    if (!stack.push(*atom))
      return Error(REG_ESPACE);
    ++atom_count;
  }

  ErrorOr<ExprId> acc = pool.empty_str();
  if (!acc.has_value())
    return acc;
  for (; atom_count > 0; --atom_count) {
    acc = pool.make_concat(stack.pop(), *acc);
    if (!acc.has_value())
      return acc;
  }
  return acc;
}

// Parses an alternation expression (branches separated by '|').
ErrorOr<ExprId> parse_alt(cpp::string_view &rem, int cflags, ExprPool &pool,
                          size_t depth, size_t &re_nsub,
                          ParsedAtomStack &stack) {
  if (rem.starts_with('|'))
    return Error(REG_BADPAT);

  ErrorOr<ExprId> first =
      parse_concat(rem, cflags, pool, depth, re_nsub, stack);
  if (!first.has_value() || !rem.starts_with('|'))
    return first;
  if (!stack.push(*first))
    return Error(REG_ESPACE);
  size_t branch_count = 1;

  while (rem.starts_with('|')) {
    rem.remove_prefix(1);
    if (rem.empty() || rem.starts_with('|') ||
        (depth > 0 && rem.starts_with(')')))
      return Error(REG_BADPAT);

    ErrorOr<ExprId> branch =
        parse_concat(rem, cflags, pool, depth, re_nsub, stack);
    if (!branch.has_value())
      return branch;
    if (!stack.push(*branch))
      return Error(REG_ESPACE);
    ++branch_count;
  }

  // Fold alternation branches right-to-left so right-associative sorted
  // make_alt stays O(1) per branch without orphaned spine nodes.
  ErrorOr<ExprId> acc = EMPTY_SET_ID;
  for (; branch_count > 0; --branch_count) {
    acc = pool.make_alt(stack.pop(), *acc);
    if (!acc.has_value())
      return acc;
  }
  return acc;
}

} // namespace

// Top-level entry point: parses an ERE pattern starting from alternation at
// depth 0 and optionally returns the total count of parenthesized
// subexpressions.
ErrorOr<ExprId> parse_ere(cpp::string_view pattern, int cflags, ExprPool &pool,
                          size_t *out_re_nsub) {
  cpp::string_view rem = pattern;
  size_t re_nsub = 0;
  ParsedAtomStack stack;
  ErrorOr<ExprId> res =
      parse_alt(rem, cflags, pool, /*depth=*/0, re_nsub, stack);
  if (!res.has_value())
    return res;
  if (out_re_nsub != nullptr)
    *out_re_nsub = re_nsub;
  return res;
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL
