//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// AST nodes for Regular Expressions (Class Definitions).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_AST_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_AST_H

#include "hdr/limits_macros.h"
#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/bitset.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

/// 32-bit index identifying a unique node in an ExprPool.
using ExprId = uint32_t;
inline constexpr ExprId INVALID_EXPR_ID = 0;
inline constexpr ExprId EMPTY_SET_ID = 1;
inline constexpr ExprId EMPTY_STR_ID = 2;

/// Sentinel upper bound representing unbounded repetition in Repeat(body, m,
/// REPEAT_INF); never collides with parsed bounds (<= RE_DUP_MAX = 255).
inline constexpr uint16_t REPEAT_INF = 0xFFFF;

/// Compile-time guard against nested repetition bombs (e.g. (a{255}){255});
/// ExprPool::MAX_NODE_LIMIT bounds lazy runtime state growth.
inline constexpr uint16_t MAX_REPEAT_EXPANSION = 32768;
static_assert(MAX_REPEAT_EXPANSION < REPEAT_INF,
              "MAX_REPEAT_EXPANSION must leave headroom below REPEAT_INF");
static_assert(RE_DUP_MAX < REPEAT_INF,
              "RE_DUP_MAX must not collide with REPEAT_INF");

/// One bit per possible byte value (2^CHAR_BIT = 256), allowing O(1)
/// membership tests for wildcards and bracket expressions.
inline constexpr size_t BYTE_SET_BITS = size_t(1) << CHAR_BIT;
using ByteSet = cpp::bitset<BYTE_SET_BITS>;

/// Bitmask flags identifying zero-width boundary assertions.
enum AnchorKind : uint8_t {
  ANCHOR_BOL = 1 << 0,
  ANCHOR_EOL = 1 << 1,
};

/// Mask of all supported AnchorKind flags.
inline constexpr uint8_t ALL_ANCHORS_MASK = ANCHOR_BOL | ANCHOR_EOL;
static_assert(ALL_ANCHORS_MASK == 0x3,
              "4-bit nullable_mask covers exactly 2 anchor bits");
/// 4-bit nullability truth table masks across anchor_ctx values 0..3.
inline constexpr uint8_t NULLABLE_NEVER = 0x0;
inline constexpr uint8_t NULLABLE_ALWAYS = 0xF;

/// Enumeration of Regular Expression AST node types.
enum class ExprKind : uint8_t {
  /// Represents the empty set (matches nothing).
  EmptySet,
  /// Represents the empty string (matches the empty string).
  EmptyStr,
  /// A literal character match.
  Literal,
  /// A zero-width boundary assertion (^ or $).
  Anchor,
  /// A 256-bit character class or wildcard match.
  ByteSet,
  /// Concatenation of two expressions (left followed by right).
  Concat,
  /// Alternation between two expressions (left or right).
  Alt,
  /// Zero or more repetitions of a sub-expression (body*).
  Star,
  /// Bounded or lower-bounded repetitions of a sub-expression (body{min,max}).
  Repeat,
};

/// A node in the Regular Expression Abstract Syntax Tree.
///
/// Expressions are represented as a hash-consed DAG to enable efficient
/// derivative-based matching. This structure is intended to be managed by
/// an ExprPool.
struct Expr {
  struct Binary {
    ExprId left;
    ExprId right;
  };
  struct RepeatBounds {
    ExprId body;
    uint16_t min;
    uint16_t max;
  };

  /// The type of this expression node.
  ExprKind kind = ExprKind::EmptySet;
  /// 4-bit truth table of empty-string nullability across anchor_ctx 0..3
  /// (bit c is 1 iff this node matches empty when active anchors == c).
  uint8_t nullable_mask : 4;
  /// Whether this expression or any child contains a Repeat node.
  bool has_repeat : 1;
  /// Saturated cumulative repetition multiplier for O(1) DAG expansion checks.
  uint16_t rep_expansion = 1;
  /// Precomputed 32-bit structural hash for O(1) lookup and rehashing.
  uint32_t hash = 0;
  union {
    /// Sub-expressions for Concat and Alt nodes.
    Binary bin = {INVALID_EXPR_ID, INVALID_EXPR_ID};
    /// Sub-expression and bounds for Repeat nodes.
    RepeatBounds rep;
    /// Sub-expression for Star nodes.
    ExprId body;
    /// Index into ExprPool::byte_sets for ByteSet nodes.
    uint32_t byte_set_id;
    /// Bitmask of AnchorKind flags for Anchor nodes.
    uint8_t anchor_mask;
    /// The character for Literal nodes.
    char ch;
  };

  /// Default constructor creates an EmptySet node.
  /// Public to allow array allocation in ExprPool::Block.
  constexpr Expr() : nullable_mask(NULLABLE_NEVER), has_repeat(false) {}

private:
  /// Computes the 4-bit nullability truth table for an Anchor bitmask.
  static constexpr uint8_t anchor_nullable_mask(uint8_t mask) {
    uint8_t out = 0;
    for (uint8_t ctx = 0; ctx < 4; ++ctx)
      if ((ctx & mask) == mask)
        out |= static_cast<uint8_t>(1u << ctx);
    return out;
  }

  /// Create a leaf node with a specific nullability mask (and optional char).
  constexpr Expr(ExprKind k, uint8_t null_mask, char c = '\0')
      : kind(k), nullable_mask(null_mask), has_repeat(false), ch(c) {}
  /// Create an Anchor node.
  constexpr Expr(uint8_t mask)
      : kind(ExprKind::Anchor), nullable_mask(anchor_nullable_mask(mask)),
        has_repeat(false), anchor_mask(mask) {}
  /// Create a ByteSet node.
  constexpr Expr(uint32_t bs_id)
      : kind(ExprKind::ByteSet), nullable_mask(NULLABLE_NEVER),
        has_repeat(false), byte_set_id(bs_id) {}
  /// Create a Star node.
  constexpr Expr(ExprId b, bool repeats, uint16_t exp)
      : kind(ExprKind::Star), nullable_mask(NULLABLE_ALWAYS),
        has_repeat(repeats), rep_expansion(exp), body(b) {}
  /// Create a Repeat node.
  constexpr Expr(ExprId b, uint16_t min_rep, uint16_t max_rep,
                 uint8_t body_null_mask, uint16_t exp)
      : kind(ExprKind::Repeat),
        nullable_mask((min_rep == 0) ? NULLABLE_ALWAYS : body_null_mask),
        has_repeat(true), rep_expansion(exp), rep{b, min_rep, max_rep} {}
  /// Create a binary node (Concat or Alt).
  constexpr Expr(ExprKind k, uint8_t null_mask, bool repeats, uint16_t exp,
                 ExprId l, ExprId r)
      : kind(k), nullable_mask(null_mask), has_repeat(repeats),
        rep_expansion(exp), bin{l, r} {}

public:
  static constexpr Expr make_empty_set() {
    return Expr(ExprKind::EmptySet, NULLABLE_NEVER);
  }
  static constexpr Expr make_empty_str() {
    return Expr(ExprKind::EmptyStr, NULLABLE_ALWAYS);
  }
  static constexpr Expr make_literal(char c) {
    return Expr(ExprKind::Literal, NULLABLE_NEVER, c);
  }
  static constexpr Expr make_anchor(uint8_t mask) { return Expr(mask); }
  static constexpr Expr make_byte_set(uint32_t bs_id) { return Expr(bs_id); }
  static constexpr Expr make_star(ExprId b, bool repeats = false,
                                  uint16_t exp = 1) {
    return Expr(b, repeats, exp);
  }
  static constexpr Expr make_repeat(ExprId b, uint16_t min_rep,
                                    uint16_t max_rep,
                                    uint8_t body_null_mask = NULLABLE_NEVER,
                                    uint16_t exp = 1) {
    return Expr(b, min_rep, max_rep, body_null_mask, exp);
  }
  static constexpr Expr make_concat(ExprId l, ExprId r,
                                    uint8_t null_mask = NULLABLE_NEVER,
                                    bool repeats = false, uint16_t exp = 1) {
    return Expr(ExprKind::Concat, null_mask, repeats, exp, l, r);
  }
  static constexpr Expr make_alt(ExprId l, ExprId r,
                                 uint8_t null_mask = NULLABLE_NEVER,
                                 bool repeats = false, uint16_t exp = 1) {
    return Expr(ExprKind::Alt, null_mask, repeats, exp, l, r);
  }

  /// Equivalence check for hash-consing.
  bool operator==(const Expr &other) const {
    if (kind != other.kind)
      return false;
    switch (kind) {
    case ExprKind::EmptySet:
    case ExprKind::EmptyStr:
      return true;
    case ExprKind::Literal:
      return ch == other.ch;
    case ExprKind::Anchor:
      return anchor_mask == other.anchor_mask;
    case ExprKind::ByteSet:
      return byte_set_id == other.byte_set_id;
    case ExprKind::Star:
      return body == other.body;
    case ExprKind::Repeat:
      return rep.body == other.rep.body && rep.min == other.rep.min &&
             rep.max == other.rep.max;
    case ExprKind::Concat:
    case ExprKind::Alt:
      return bin.left == other.bin.left && bin.right == other.bin.right;
    }
    __builtin_unreachable();
  }
};
static_assert(sizeof(Expr) == 16, "Expr must remain 16 bytes");

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_AST_H
