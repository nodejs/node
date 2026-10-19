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
#include "src/__support/CPP/utility.h"
#include "src/__support/alloc-checker.h"
#include "src/__support/hash.h"
#include "src/__support/macros/config.h"
#include "src/string/memory_utils/inline_memset.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

namespace {

// Hash an Expr node for hash-consing.
uint32_t hash_expr(const Expr &e) {
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
  uint64_t h = hasher.finish();
  return static_cast<uint32_t>(h ^ (h >> 32));
}

// Bounded linear-probing range over a power-of-two open-addressing table.
class BucketProber {
  cpp::span<ExprId> table;
  size_t mask;
  size_t start_idx;

public:
  struct Iterator {
    cpp::span<ExprId> table;
    size_t mask;
    size_t idx;
    size_t step;

    ExprId &operator*() const { return table[idx]; }
    Iterator &operator++() {
      idx = (idx + 1) & mask;
      ++step;
      return *this;
    }
    constexpr bool operator!=(const Iterator &other) const {
      return step != other.step;
    }
  };

  BucketProber(cpp::span<ExprId> t, uint32_t hash)
      : table(t), mask(t.size() - 1), start_idx(hash & mask) {
    LIBC_ASSERT(!t.empty() && (t.size() & mask) == 0);
  }

  Iterator begin() const { return {table, mask, start_idx, 0}; }
  Iterator end() const { return {table, mask, start_idx, table.size()}; }
};

} // namespace

// TODO: When cpp::unique_ptr and cpp::make_unique are added to
// src/__support/CPP/, replace the manual AllocChecker + new[]/delete[] calls in
// ensure_initialized(), ~ExprPool(), allocate_block(), and grow_hashtable()
// with cpp::make_unique<ExprId[]>(...) and cpp::make_unique<Block>(),
// eliminating explicit zeroing via inline_memset and the destructor cleanup
// loop.
ExprPool::~ExprPool() {
  delete[] hashtable.data();
  for (Block *blk : active_blocks())
    delete blk;
  delete[] blocks.data();
}

bool ExprPool::ensure_initialized() {
  if (node_count >= EMPTY_STR_ID)
    return true;
  if (hashtable.empty()) {
    AllocChecker ac;
    ExprId *raw_table = new (ac) ExprId[INITIAL_HASH_CAPACITY];
    if (!ac)
      return false;
    hashtable = cpp::span<ExprId>(raw_table, INITIAL_HASH_CAPACITY);
    inline_memset(hashtable.data(), 0, hashtable.size_bytes());
  }
  if (block_count == 0 && !allocate_block())
    return false;

  auto empty_set_id = intern(Expr::make_empty_set());
  auto empty_str_id = intern(Expr::make_empty_str());
  if (!empty_set_id.has_value() || !empty_str_id.has_value())
    return false;
  LIBC_ASSERT(*empty_set_id == EMPTY_SET_ID);
  LIBC_ASSERT(*empty_str_id == EMPTY_STR_ID);
  return true;
}

ExprId &ExprPool::find_bucket(cpp::span<ExprId> table, const Expr &e) const {
  for (ExprId &slot : BucketProber(table, e.hash)) {
    if (slot == INVALID_EXPR_ID)
      return slot;
    const Expr &cand = get(slot);
    if (cand.hash == e.hash && cand == e)
      return slot;
  }
  __builtin_unreachable();
}

bool ExprPool::allocate_block() {
  AllocChecker ac;
  if (block_count == blocks.size()) {
    size_t new_cap =
        blocks.empty() ? INITIAL_BLOCKS_CAPACITY : blocks.size() * 2;
    Block **new_blocks = new (ac) Block *[new_cap];
    if (!ac)
      return false;
    Block **dst = new_blocks;
    for (Block *blk : active_blocks()) {
      *dst = blk;
      ++dst;
    }
    delete[] blocks.data();
    blocks = cpp::span<Block *>(new_blocks, new_cap);
  }
  Block *blk = new (ac) Block();
  if (!ac)
    return false;
  blocks[block_count] = blk;
  ++block_count;
  return true;
}

bool ExprPool::grow_hashtable() {
  size_t new_cap = hashtable.size() * 2;
  AllocChecker ac;
  ExprId *raw_table = new (ac) ExprId[new_cap];
  if (!ac)
    return false;
  cpp::span<ExprId> new_table(raw_table, new_cap);
  inline_memset(new_table.data(), 0, new_table.size_bytes());

  for (ExprId id : hashtable)
    if (id != INVALID_EXPR_ID)
      find_bucket(new_table, get(id)) = id;

  delete[] hashtable.data();
  hashtable = new_table;
  return true;
}

ErrorOr<ExprId> ExprPool::intern(Expr e) {
  if ((hashtable.empty() || block_count == 0) && !ensure_initialized())
    return Error(REG_ESPACE);

  // Probe for an existing node with identical content.
  e.hash = hash_expr(e);
  ExprId *bucket = &find_bucket(hashtable, e);
  if (*bucket != INVALID_EXPR_ID)
    return *bucket;

  // Admission Control: Check the hard limit on AST nodes.
  if (node_count >= MAX_NODE_LIMIT)
    return Error(REG_ESPACE);

  if (node_count >= max_hashtable_nodes()) {
    if (!grow_hashtable())
      return Error(REG_ESPACE);
    bucket = &find_bucket(hashtable, e);
  }

  // Arena Allocation: If no matching node found, allocate a stable slot.
  if (node_count == block_count * NODES_PER_BLOCK) {
    if (!allocate_block())
      return Error(REG_ESPACE);
  }

  // Node Initialisation: Copy the structural definition into the arena.
  ++node_count;
  ExprId new_id = static_cast<ExprId>(node_count);
  slot_at(new_id) = e;
  *bucket = new_id;
  return new_id;
}

ErrorOr<ExprId> ExprPool::empty_set() {
  if (!ensure_initialized())
    return Error(REG_ESPACE);
  return EMPTY_SET_ID;
}
ErrorOr<ExprId> ExprPool::empty_str() {
  if (!ensure_initialized())
    return Error(REG_ESPACE);
  return EMPTY_STR_ID;
}
ErrorOr<ExprId> ExprPool::make_lit(char c) {
  return intern(Expr::make_literal(c));
}

ErrorOr<ExprId> ExprPool::make_concat(ExprId l, ExprId r) {
  if (l == INVALID_EXPR_ID || r == INVALID_EXPR_ID)
    return Error(REG_BADPAT);
  // Apply basic algebraic identities for concatenation:
  // Ø · R = R · Ø = Ø (Identity: null set)
  if (l == EMPTY_SET_ID || r == EMPTY_SET_ID)
    return empty_set();
  // ε · R = R · ε = R (Identity: empty string)
  if (l == EMPTY_STR_ID)
    return r;
  if (r == EMPTY_STR_ID)
    return l;
  return intern(Expr::make_concat(l, r, is_nullable(l) && is_nullable(r)));
}
ErrorOr<ExprId> ExprPool::make_alt(ExprId l, ExprId r) {
  if (l == INVALID_EXPR_ID || r == INVALID_EXPR_ID)
    return Error(REG_BADPAT);
  // Apply basic algebraic identities for alternation:
  // Ø | R = R | Ø = R (Identity: null set)
  if (l == EMPTY_SET_ID)
    return r;
  if (r == EMPTY_SET_ID)
    return l;
  // R | R = R (Idempotency)
  if (l == r)
    return l;
  if (l > r)
    cpp::swap(l, r);
  return intern(Expr::make_alt(l, r, is_nullable(l) || is_nullable(r)));
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL
