//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Pool for Regular Expression AST nodes (Implementation).
///
//===----------------------------------------------------------------------===//

#include "src/__support/regex/regex_expr_pool.h"
#include "hdr/regex_macros.h"
#include "src/__support/CPP/new.h"
#include "src/__support/alloc-checker.h"
#include "src/__support/hash.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/memory_utils/inline_memset.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

namespace {

// Hash an Expr node for hash-consing.
uint64_t hash_expr(const Expr &e) {
  // Initialise HashState with a constant seed. The specific value (0x12345678)
  // is an arbitrary placeholder; HashState immediately mixes this seed with
  // high-entropy constants (derived from aHash) to produce a strong hash, while
  // the constant value guarantees deterministic hashing for hash-consing.
  internal::HashState hasher(0x12345678);
  uint64_t kind = static_cast<uint64_t>(e.kind);
  hasher.update(&kind, sizeof(kind));
  switch (e.kind) {
  case ExprKind::Literal:
    hasher.update(&e.ch, sizeof(e.ch));
    break;
  case ExprKind::Concat:
  case ExprKind::Alt:
    hasher.update(&e.bin.left, sizeof(e.bin.left));
    hasher.update(&e.bin.right, sizeof(e.bin.right));
    break;
  default:
    break;
  }
  return hasher.finish();
}

} // namespace

ExprPool::ExprPool() {
  AllocChecker ac;
  hashtable = new (ac) Expr *[HASH_TABLE_SIZE];
  if (ac) {
    inline_memset(hashtable, 0, HASH_TABLE_SIZE * sizeof(Expr *));
  }
}

ExprPool::~ExprPool() {
  if (hashtable)
    delete[] hashtable;
  Block *b = head;
  while (b) {
    Block *next_b = b->next;
    delete b;
    b = next_b;
  }
}

cpp::expected<Expr *, int> ExprPool::intern(const Expr &e) {
  if (!hashtable)
    return cpp::unexpected(REG_ESPACE);

  // 1. Calculate the initial bucket for the given structural definition.
  uint64_t h = hash_expr(e);
  size_t idx = h % HASH_TABLE_SIZE;

  // 2. Linear Probing: Search for an existing node with identical content.
  //    Because pointers are unique, O(1) comparison is guaranteed if
  //    sub-expressions are already interned.
  size_t start_idx = idx;
  while (hashtable[idx]) {
    if (*hashtable[idx] == e)
      return hashtable[idx];
    idx = (idx + 1) % HASH_TABLE_SIZE;
    if (idx == start_idx) {
      // Table full (invariant check: HASH_TABLE_SIZE >> MAX_NODE_LIMIT)
      return cpp::unexpected(REG_ESPACE);
    }
  }

  // 3. Admission Control: Check the hard limit on AST nodes.
  if (node_count >= MAX_NODE_LIMIT)
    return cpp::unexpected(REG_ESPACE);

  // 4. Arena Allocation: If no matching node found, allocate a stable slot.
  if (!current || current->used == Block::BLOCK_SIZE) {
    // New blocks are allocated on demand using AllocChecker.
    AllocChecker ac;
    Block *new_block = new (ac) Block();
    if (!ac)
      return cpp::unexpected(REG_ESPACE);
    if (!head)
      head = new_block;
    if (current)
      current->next = new_block;
    current = new_block;
  }

  // 5. Node Initialisation: Copy the structural definition into the arena.
  Expr *new_node = &current->nodes[current->used];
  ++current->used;
  *new_node = e;
  hashtable[idx] = new_node;
  node_count++;
  return new_node;
}

cpp::expected<Expr *, int> ExprPool::empty_set() {
  return intern(Expr::make_empty_set());
}
cpp::expected<Expr *, int> ExprPool::empty_str() {
  return intern(Expr::make_empty_str());
}
cpp::expected<Expr *, int> ExprPool::make_lit(char c) {
  return intern(Expr::make_literal(c));
}

cpp::expected<Expr *, int> ExprPool::make_concat(Expr *l, Expr *r) {
  if (!l || !r)
    return cpp::unexpected(REG_BADPAT);
  // Apply basic algebraic identities for concatenation:
  // 1. Ø · R = R · Ø = Ø (Identity: null set)
  if (l->kind == ExprKind::EmptySet || r->kind == ExprKind::EmptySet)
    return empty_set();
  // 2. ε · R = R · ε = R (Identity: empty string)
  if (l->kind == ExprKind::EmptyStr)
    return r;
  if (r->kind == ExprKind::EmptyStr)
    return l;
  return intern(Expr::make_concat(l, r));
}
cpp::expected<Expr *, int> ExprPool::make_alt(Expr *l, Expr *r) {
  if (!l || !r)
    return cpp::unexpected(REG_BADPAT);
  // Apply basic algebraic identities for alternation:
  // 1. Ø | R = R | Ø = R (Identity: null set)
  if (l->kind == ExprKind::EmptySet)
    return r;
  if (r->kind == ExprKind::EmptySet)
    return l;
  // 2. R | R = R (Idempotency)
  if (l == r)
    return l;
  return intern(Expr::make_alt(l, r));
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL
