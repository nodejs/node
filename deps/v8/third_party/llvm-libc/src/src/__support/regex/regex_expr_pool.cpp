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
#include "src/__support/CPP/scope.h"
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
  case ExprKind::Anchor:
    hasher.update(&e.anchor_mask, sizeof(e.anchor_mask));
    break;
  case ExprKind::ByteSet:
    hasher.update(&e.byte_set_id, sizeof(e.byte_set_id));
    break;
  case ExprKind::Star:
    hasher.update(&e.body, sizeof(e.body));
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
  size_t start_idx;

  constexpr size_t mask() const { return table.size() - 1; }

public:
  struct Sentinel {};

  struct Iterator {
    cpp::span<ExprId> table;
    size_t idx;
    size_t step;

    constexpr size_t mask() const { return table.size() - 1; }

    ExprId &operator*() const { return table[idx]; }
    Iterator &operator++() {
      idx = (idx + 1) & mask();
      ++step;
      return *this;
    }
    constexpr bool operator!=(Sentinel) const { return step != table.size(); }
  };

  BucketProber(cpp::span<ExprId> t, uint32_t hash)
      : table(t), start_idx(hash & (t.size() - 1)) {
    LIBC_ASSERT(!t.empty() && (t.size() & mask()) == 0);
  }

  Iterator begin() const { return {table, start_idx, 0}; }
  Sentinel end() const { return {}; }
};

} // namespace

// TODO: When cpp::unique_ptr and cpp::make_unique are added to
// src/__support/CPP/, replace the manual AllocChecker + new[]/delete[] calls in
// ensure_initialized(), ~ExprPool(), allocate_block(), make_byte_set(), and
// grow_hashtable() with cpp::make_unique<ExprId[]>(...) and
// cpp::make_unique<Block>(), eliminating explicit zeroing via inline_memset and
// the destructor cleanup loop.
ExprPool::~ExprPool() {
  delete[] byte_sets.data();
  delete[] hashtable.data();
  for (Block *blk : active_blocks())
    delete blk;
  delete[] blocks.data();
}

bool ExprPool::ensure_initialized() {
  if (block_count > 0)
    return true;
  AllocChecker ac;
  ExprId *raw_table = new (ac) ExprId[INITIAL_HASH_CAPACITY];
  if (!ac)
    return false;
  cpp::scope_exit table_guard([raw_table] { delete[] raw_table; });
  if (!allocate_block())
    return false;
  table_guard.release();
  hashtable = cpp::span<ExprId>(raw_table, INITIAL_HASH_CAPACITY);
  inline_memset(hashtable.data(), 0, hashtable.size_bytes());

  [[maybe_unused]] auto empty_set_id = intern(Expr::make_empty_set());
  [[maybe_unused]] auto empty_str_id = intern(Expr::make_empty_str());
  LIBC_ASSERT(empty_set_id.has_value() && *empty_set_id == EMPTY_SET_ID);
  LIBC_ASSERT(empty_str_id.has_value() && *empty_str_id == EMPTY_STR_ID);
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
    size_t new_cap = blocks.empty() ? INITIAL_BLOCKS_CAPACITY
                                    : blocks.size() * GROWTH_FACTOR;
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
  size_t new_cap = hashtable.size() * GROWTH_FACTOR;
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
  if (!ensure_initialized())
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
ErrorOr<ExprId> ExprPool::make_anchor(uint8_t mask) {
  return intern(Expr::make_anchor(mask));
}

ErrorOr<ExprId> ExprPool::make_byte_set(const ByteSet &bs) {
  size_t count = 0;
  size_t last_bit = 0;
  for (size_t b = 0; b < BYTE_SET_BITS; ++b) {
    if (bs.test(b)) {
      ++count;
      last_bit = b;
    }
  }
  if (count == 0)
    return empty_set();
  if (count == 1)
    return make_lit(static_cast<char>(last_bit));

  uint32_t bs_id = 0;
  for (const ByteSet &existing : active_byte_sets()) {
    if (existing == bs)
      return intern(Expr::make_byte_set(bs_id));
    ++bs_id;
  }

  if (byte_set_count == byte_sets.size()) {
    size_t new_cap = byte_sets.empty() ? INITIAL_BYTE_SETS_CAPACITY
                                       : byte_sets.size() * GROWTH_FACTOR;
    AllocChecker ac;
    ByteSet *new_sets = new (ac) ByteSet[new_cap];
    if (!ac)
      return Error(REG_ESPACE);
    ByteSet *dst = new_sets;
    for (const ByteSet &existing : active_byte_sets()) {
      *dst = existing;
      ++dst;
    }
    delete[] byte_sets.data();
    byte_sets = cpp::span<ByteSet>(new_sets, new_cap);
  }

  ErrorOr<ExprId> id = intern(Expr::make_byte_set(bs_id));
  if (!id.has_value())
    return id;
  byte_sets[byte_set_count] = bs;
  ++byte_set_count;
  return id;
}

ErrorOr<ExprId> ExprPool::make_star(ExprId body) {
  if (body == INVALID_EXPR_ID)
    return Error(REG_BADPAT);
  // Ø* = ε and ε* = ε
  if (body == EMPTY_SET_ID || body == EMPTY_STR_ID)
    return empty_str();
  // (R*)* = R* (Idempotency)
  if (get(body).kind == ExprKind::Star)
    return body;
  return intern(Expr::make_star(body, has_anchors(body)));
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
  // (A · B) · C = A · (B · C) (Right-associative canonical form)
  const Expr le = get(l);
  if (le.kind == ExprKind::Concat) {
    ErrorOr<ExprId> tail = make_concat(le.bin.right, r);
    if (!tail.has_value())
      return tail;
    return make_concat(le.bin.left, *tail);
  }
  return intern(Expr::make_concat(l, r, is_nullable(l) && is_nullable(r),
                                  has_anchors(l) || has_anchors(r)));
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

  const Expr le = get(l);
  const Expr re = get(r);
  if (le.kind != ExprKind::Alt && re.kind != ExprKind::Alt) {
    if (l > r)
      cpp::swap(l, r);
    return intern(Expr::make_alt(l, r, is_nullable(l) || is_nullable(r),
                                 has_anchors(l) || has_anchors(r)));
  }

  // Merge two right-associated sorted Alt spines in ascending ExprId order so
  // multi-way alternations canonicalize to a single deduplicated tree and
  // derivative state spaces under '*' stay finite.
  ExprId hl = (le.kind == ExprKind::Alt) ? le.bin.left : l;
  ExprId tl = (le.kind == ExprKind::Alt) ? le.bin.right : EMPTY_SET_ID;
  ExprId hr = (re.kind == ExprKind::Alt) ? re.bin.left : r;
  ExprId tr = (re.kind == ExprKind::Alt) ? re.bin.right : EMPTY_SET_ID;
  ExprId head = hl;
  ErrorOr<ExprId> rest = EMPTY_SET_ID;
  if (hl == hr) {
    rest = make_alt(tl, tr);
  } else if (hl < hr) {
    rest = make_alt(tl, r);
  } else {
    head = hr;
    rest = make_alt(l, tr);
  }
  if (!rest.has_value())
    return rest;
  return intern(Expr::make_alt(head, *rest,
                               is_nullable(head) || is_nullable(*rest),
                               has_anchors(head) || has_anchors(*rest)));
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL
