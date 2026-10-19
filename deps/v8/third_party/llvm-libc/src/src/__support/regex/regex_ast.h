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

#include "hdr/stdint_proxy.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

/// 32-bit index identifying a unique node in an ExprPool.
using ExprId = uint32_t;
inline constexpr ExprId INVALID_EXPR_ID = 0;
inline constexpr ExprId EMPTY_SET_ID = 1;
inline constexpr ExprId EMPTY_STR_ID = 2;

/// Enumeration of Regular Expression AST node types.
enum class ExprKind : uint8_t {
  /// Represents the empty set (matches nothing).
  EmptySet,
  /// Represents the empty string (matches the empty string).
  EmptyStr,
  /// A literal character match.
  Literal,
  /// Concatenation of two expressions (left followed by right).
  Concat,
  /// Alternation between two expressions (left or right).
  Alt,
};

/// A node in the Regular Expression Abstract Syntax Tree.
///
/// Expressions are represented as a hash-consed DAG to enable efficient
/// derivative-based matching. This structure is intended to be managed by
/// an ExprPool.
struct Expr {
  /// The type of this expression node.
  ExprKind kind = ExprKind::EmptySet;
  /// Whether this expression can match the empty string.
  bool nullable = false;
  /// Character value for Literal nodes.
  char ch = '\0';
  /// Precomputed 32-bit structural hash for O(1) lookup and rehashing.
  uint32_t hash = 0;
  /// Sub-expressions for Concat and Alt nodes.
  struct {
    ExprId left;
    ExprId right;
  } bin = {INVALID_EXPR_ID, INVALID_EXPR_ID};

  /// Default constructor creates an EmptySet node.
  /// Public to allow array allocation in ExprPool::Block.
  constexpr Expr() = default;

private:
  /// Create a node of a specific kind with nullability.
  constexpr Expr(ExprKind k, bool is_null) : kind(k), nullable(is_null) {}
  /// Create a Literal node.
  constexpr Expr(char c) : kind(ExprKind::Literal), ch(c) {}
  /// Create a binary node (Concat or Alt).
  constexpr Expr(ExprKind k, bool is_null, ExprId l, ExprId r)
      : kind(k), nullable(is_null), bin{l, r} {}

public:
  static constexpr Expr make_empty_set() {
    return Expr(ExprKind::EmptySet, false);
  }
  static constexpr Expr make_empty_str() {
    return Expr(ExprKind::EmptyStr, true);
  }
  static constexpr Expr make_literal(char c) { return Expr(c); }
  static constexpr Expr make_concat(ExprId l, ExprId r, bool is_null = false) {
    return Expr(ExprKind::Concat, is_null, l, r);
  }
  static constexpr Expr make_alt(ExprId l, ExprId r, bool is_null = false) {
    return Expr(ExprKind::Alt, is_null, l, r);
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
    case ExprKind::Concat:
    case ExprKind::Alt:
      return bin.left == other.bin.left && bin.right == other.bin.right;
    }
    return false;
  }
};
static_assert(sizeof(Expr) == 16, "Expr must remain 16 bytes");

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_AST_H
