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
#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/utility.h"
#include "src/__support/blockstore.h"
#include "src/__support/checked_ptr.h"
#include "src/__support/hash.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

namespace {

// Hash an Expr node for hash-consing using the pool's pre-mixed seed.
uint32_t hash_expr(const Expr e, uint32_t seed) {
  const uint64_t s = (static_cast<uint64_t>(seed) << 32) | seed;
  internal::HashState hasher(
      internal::RANDOMNESS[1][0] ^ s, internal::RANDOMNESS[1][1] ^ s,
      internal::RANDOMNESS[1][2] ^ s, internal::RANDOMNESS[1][3] ^ s);
  struct HashKey {
    uint64_t kind;
    uint64_t payload;
  } key = {static_cast<uint64_t>(e.kind), 0};
  static_assert(sizeof(HashKey) == 2 * sizeof(uint64_t));
  switch (e.kind) {
  case ExprKind::EmptySet:
  case ExprKind::EmptyStr:
    break;
  case ExprKind::Literal:
    key.payload = static_cast<uint8_t>(e.ch);
    break;
  case ExprKind::Anchor:
    key.payload = e.anchor_mask;
    break;
  case ExprKind::ByteSet:
    key.payload = e.byte_set_id;
    break;
  case ExprKind::Star:
    key.payload = e.body;
    break;
  case ExprKind::Repeat:
    key.payload = (static_cast<uint64_t>(e.rep.body) << 32) |
                  (static_cast<uint64_t>(e.rep.min) << 16) | e.rep.max;
    break;
  case ExprKind::Concat:
  case ExprKind::Alt:
    key.payload = (static_cast<uint64_t>(e.bin.left) << 32) | e.bin.right;
    break;
  }
  hasher.update(&key, sizeof(key));
  uint64_t h = hasher.finish();
  return static_cast<uint32_t>(h ^ (h >> 32));
}

// Bounded triangular-probing range over a power-of-two open-addressing table.
// Triangular numbers k(k+1)/2 mod 2^p visit every bucket exactly once while
// avoiding linear-probing primary clustering.
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
      // Advance by increasing step sizes (triangular probing) rather than
      // sequentially; visiting buckets out of order is intentional.
      ++step;
      idx = (idx + step) & mask();
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

constexpr size_t INLINE_SPINE_CAPACITY = 16;

// RAII LIFO wrapper around ReverseOrderBlockStore so stack-local spine buffers
// keep short spines stack-only (INLINE_SPINE_CAPACITY inline ExprIds, zero heap
// allocations), iterate in reverse (right-to-left) natively, and always release
// spilled blocks on early returns.
struct ScopedReverseExprIdStack
    : ReverseOrderBlockStore<ExprId, INLINE_SPINE_CAPACITY> {
  ScopedReverseExprIdStack() = default;
  ScopedReverseExprIdStack(const ScopedReverseExprIdStack &) = delete;
  ScopedReverseExprIdStack &
  operator=(const ScopedReverseExprIdStack &) = delete;
  ~ScopedReverseExprIdStack() { destroy(this); }
};

// Pops the head branch from a right-associated Alt spine (or a single non-Alt
// node), advancing `spine` to the remaining tail or EMPTY_SET_ID.
ExprId pop_alt_head(ExprId &spine, const ExprPool &pool) {
  LIBC_ASSERT(spine != INVALID_EXPR_ID && spine != EMPTY_SET_ID);
  const Expr e = pool.get(spine);
  if (e.kind == ExprKind::Alt) {
    spine = e.bin.right;
    return e.bin.left;
  }
  ExprId head = spine;
  spine = EMPTY_SET_ID;
  return head;
}

} // namespace

struct ExprPool::ReverseExprIdStack : ScopedReverseExprIdStack {};

bool ExprPool::ensure_initialized() {
  if (block_count > 0)
    return true;
  auto table = make_unique<ExprId[]>(INITIAL_HASH_CAPACITY);
  if (!table)
    return false;
  if (!allocate_block())
    return false;
  hashtable = table.take_unique();
  hashtable_capacity = INITIAL_HASH_CAPACITY;

  // Seed per-pool hashing from both heap and stack addresses without syscalls
  // so baremetal and GPU targets stay self-contained.  Do not XOR this with
  // blocks.get(): regcomp() allocates RegexInternal on the heap right before
  // blocks, so both pointers share the same heap ASLR base and XORing them
  // cancels the randomised upper bits into a constant offset.  Feeding them
  // separately into HashState alongside a stack address (&ptrs) preserves both
  // heap and stack ASLR entropy while sequential ExprIds stay deterministic.
  const cpp::array<uintptr_t, 2> ptrs = {
      reinterpret_cast<uintptr_t>(blocks.get()),
      reinterpret_cast<uintptr_t>(&ptrs)};
  internal::HashState seed_hasher(
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this)));
  seed_hasher.update(ptrs.data(), sizeof(ptrs));
  const uint64_t mixed = seed_hasher.finish();
  hash_seed = static_cast<uint32_t>(mixed ^ (mixed >> 32));

  [[maybe_unused]] auto empty_set_id = intern(Expr::make_empty_set());
  [[maybe_unused]] auto empty_str_id = intern(Expr::make_empty_str());
  LIBC_ASSERT(empty_set_id.has_value() && *empty_set_id == EMPTY_SET_ID);
  LIBC_ASSERT(empty_str_id.has_value() && *empty_str_id == EMPTY_STR_ID);
  return true;
}

ExprId &ExprPool::find_bucket(cpp::span<ExprId> table, const Expr e) const {
  for (ExprId &slot : BucketProber(table, e.hash)) {
    if (slot == INVALID_EXPR_ID)
      return slot;
    const Expr cand = get(slot);
    if (cand.hash == e.hash && cand == e)
      return slot;
  }
  LIBC_ASSERT(false);
  __builtin_unreachable();
}

bool ExprPool::allocate_block() {
  if (block_count == block_capacity) {
    size_t new_cap = block_capacity == 0 ? INITIAL_BLOCKS_CAPACITY
                                         : block_capacity * GROWTH_FACTOR;
    auto new_blocks = make_unique<cpp::unique_ptr<Block>[]>(new_cap);
    if (!new_blocks)
      return false;
    for (size_t i = 0; i < block_count; ++i)
      new_blocks[i] = cpp::move(blocks[i]);
    blocks = new_blocks.take_unique();
    block_capacity = new_cap;
  }
  auto blk = make_unique<Block>();
  if (!blk)
    return false;
  blocks[block_count] = blk.take_unique();
  ++block_count;
  return true;
}

bool ExprPool::grow_hashtable() {
  size_t new_cap = hashtable_capacity * GROWTH_FACTOR;
  auto new_table = make_unique<ExprId[]>(new_cap);
  if (!new_table)
    return false;

  cpp::span<ExprId> new_slots(new_table.get(), new_cap);
  for (ExprId id : hashtable_slots())
    if (id != INVALID_EXPR_ID)
      find_bucket(new_slots, get(id)) = id;

  hashtable = new_table.take_unique();
  hashtable_capacity = new_cap;
  return true;
}

ErrorOr<ExprId> ExprPool::intern(Expr e) {
  if (!ensure_initialized())
    return Error(REG_ESPACE);

  // Probe for an existing node with identical content.
  e.hash = hash_expr(e, hash_seed);
  ExprId *bucket = &find_bucket(hashtable_slots(), e);
  if (*bucket != INVALID_EXPR_ID)
    return *bucket;

  // Admission Control: Check the hard limit on AST nodes.
  if (node_count >= MAX_NODE_LIMIT)
    return Error(REG_ESPACE);

  if (node_count >= max_hashtable_nodes()) {
    if (!grow_hashtable())
      return Error(REG_ESPACE);
    bucket = &find_bucket(hashtable_slots(), e);
  }

  // Arena Allocation: If no matching node found, allocate a stable slot.
  if (node_count == block_count * NODES_PER_BLOCK) {
    if (!allocate_block())
      return Error(REG_ESPACE);
  }

  // Node Initialisation: Copy the structural definition into the arena.
  ++node_count;
  ExprId new_id = node_count;
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
  if (mask == 0 || (mask & ~ALL_ANCHORS_MASK) != 0)
    return Error(REG_BADPAT);
  return intern(Expr::make_anchor(mask));
}

ErrorOr<ExprId> ExprPool::make_byte_set(const ByteSet &bs) {
  size_t count = 0;
  size_t last_bit = 0;
  for (size_t b = 0; b < BYTE_SET_BITS; ++b) {
    if (bs.test(b)) {
      ++count;
      last_bit = b;
      if (count > 1)
        break;
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

  if (byte_set_count == byte_set_capacity) {
    size_t new_cap = byte_set_capacity == 0 ? INITIAL_BYTE_SETS_CAPACITY
                                            : byte_set_capacity * GROWTH_FACTOR;
    auto new_sets = make_unique<ByteSet[]>(new_cap);
    if (!new_sets)
      return Error(REG_ESPACE);
    for (size_t i = 0; i < byte_set_count; ++i)
      new_sets[i] = byte_sets[i];
    byte_sets = new_sets.take_unique();
    byte_set_capacity = new_cap;
  }

  ErrorOr<ExprId> id = intern(Expr::make_byte_set(bs_id));
  if (!id.has_value())
    return id;
  byte_sets[byte_set_count] = bs;
  ++byte_set_count;
  return id;
}

namespace {

// Multiplies a subexpression's repeat expansion factor by an enclosing repeat
// bound, saturating at MAX_REPEAT_EXPANSION + 1 so nested bounds checks never
// overflow uint16_t.
constexpr uint16_t scale_exp(uint16_t base, uint16_t factor) {
  if (factor <= 1)
    return base;
  uint32_t prod = static_cast<uint32_t>(base) * static_cast<uint32_t>(factor);
  return static_cast<uint16_t>(
      cpp::min<uint32_t>(prod, MAX_REPEAT_EXPANSION + 1));
}

} // namespace

ErrorOr<ExprId> ExprPool::make_star(ExprId body) {
  if (!is_valid_input(body))
    return Error(REG_BADPAT);
  // Ø* = ε and ε* = ε
  if (body == EMPTY_SET_ID || body == EMPTY_STR_ID)
    return empty_str();

  // Unwrap nested repeats with min <= 1: (R{m,n})* = R* iff m <= 1 (since
  // R^1 is in R{m,n} iff m <= 1, whereas (a{2,3})* excludes "a").
  Expr be = get(body);
  while (be.kind == ExprKind::Repeat && be.rep.min <= 1) {
    body = be.rep.body;
    be = get(body);
  }
  // (R*)* = R*
  if (be.kind == ExprKind::Star)
    return body;
  return intern(Expr::make_star(body, be.has_repeat, be.rep_expansion));
}

ErrorOr<ExprId> ExprPool::make_repeat(ExprId body, uint16_t min, uint16_t max) {
  if (!is_valid_input(body) || min > max || min == REPEAT_INF)
    return Error(REG_BADPAT);
  // Normalize R{0,0} = ε, ε{m,n} = ε, Ø{m,n}, R{1,1} = R, and
  // R{0,REPEAT_INF} = R*.
  if (max == 0 || body == EMPTY_STR_ID)
    return empty_str();
  if (body == EMPTY_SET_ID)
    return (min == 0) ? empty_str() : empty_set();
  if (min == 1 && max == 1)
    return body;
  if (min == 0 && max == REPEAT_INF)
    return make_star(body);
  const Expr be = get(body);
  if (be.kind == ExprKind::Star)
    return body;
  if (be.kind == ExprKind::Repeat && be.rep.min == 1 &&
      be.rep.max == REPEAT_INF)
    return make_repeat(be.rep.body, min, REPEAT_INF);
  uint16_t factor = (max == REPEAT_INF) ? min : max;
  return intern(Expr::make_repeat(body, min, max, be.nullable_mask,
                                  scale_exp(be.rep_expansion, factor)));
}

// Interns a Concat(l, r) node with propagated nullability and repeat metadata.
ErrorOr<ExprId> ExprPool::intern_concat_node(ExprId l, ExprId r) {
  const Expr le = get(l);
  const Expr re = get(r);
  return intern(Expr::make_concat(
      l, r, static_cast<uint8_t>(le.nullable_mask & re.nullable_mask),
      le.has_repeat || re.has_repeat,
      cpp::max(le.rep_expansion, re.rep_expansion)));
}

// Interns a canonical right-associated, strictly sorted Alt(l, r) node with
// propagated nullability and repeat metadata.
ErrorOr<ExprId> ExprPool::intern_alt_node(ExprId l, ExprId r) {
  const Expr le = get(l);
  const Expr re = get(r);
  LIBC_ASSERT(l != EMPTY_SET_ID && r != EMPTY_SET_ID);
  LIBC_ASSERT(le.kind != ExprKind::Alt);
  LIBC_ASSERT(l < ((re.kind == ExprKind::Alt) ? re.bin.left : r));
  return intern(Expr::make_alt(
      l, r, static_cast<uint8_t>(le.nullable_mask | re.nullable_mask),
      le.has_repeat || re.has_repeat,
      cpp::max(le.rep_expansion, re.rep_expansion)));
}

// Prepends head onto an existing Alt spine (or returns head if tail is empty).
ErrorOr<ExprId> ExprPool::prepend_alt_node(ExprId head, ExprId tail) {
  return (tail == EMPTY_SET_ID) ? ErrorOr<ExprId>(head)
                                : intern_alt_node(head, tail);
}

// Folds valid items right-to-left (LIFO) onto tail as an Alt spine, skipping
// slots cleared to INVALID_EXPR_ID during coalescing.
ErrorOr<ExprId> ExprPool::fold_alt_right(ReverseExprIdStack &items,
                                         ExprId tail) {
  ErrorOr<ExprId> acc = tail;
  for (ExprId id : items) {
    if (id == INVALID_EXPR_ID)
      continue;
    acc = prepend_alt_node(id, *acc);
    if (!acc.has_value())
      return acc;
  }
  return acc;
}

ErrorOr<ExprId> ExprPool::make_concat(ExprId l, ExprId r) {
  if (!is_valid_input(l) || !is_valid_input(r))
    return Error(REG_BADPAT);
  // Apply basic algebraic identities for concatenation:
  // Ø · R = R · Ø = Ø (Identity: null set)
  if (l == EMPTY_SET_ID || r == EMPTY_SET_ID)
    return empty_set();
  // ε · R = R · ε = R (Identity: empty string)
  if (l == EMPTY_STR_ID || r == EMPTY_STR_ID) {
    if (!ensure_initialized())
      return Error(REG_ESPACE);
    return (l == EMPTY_STR_ID) ? r : l;
  }
  // (A · B) · C = A · (B · C) (Right-associative canonical form)
  Expr ce = get(l);
  if (ce.kind != ExprKind::Concat)
    return intern_concat_node(l, r);

  // Flatten the right-associated left spine iteratively so long concatenations
  // never recurse on the call stack: (A · (B · C)) · D -> [A, B], C, D.
  ReverseExprIdStack lefts;
  ExprId cur = l;
  while (ce.kind == ExprKind::Concat) {
    if (!lefts.push_back(ce.bin.left))
      return Error(REG_ESPACE);
    cur = ce.bin.right;
    ce = get(cur);
  }

  // Rebuild right-to-left (LIFO): A · (B · (C · D)).
  ErrorOr<ExprId> acc = intern_concat_node(cur, r);
  if (!acc.has_value())
    return acc;
  for (ExprId id : lefts) {
    acc = intern_concat_node(id, *acc);
    if (!acc.has_value())
      return acc;
  }
  return acc;
}

namespace {

ErrorOr<ExprId> coalesce_repeat_leaves(ExprId a, ExprId b, ExprPool &pool);

// Coalesces R{m1,n1} | R{m2,n2} (and shared-prefix/shared-suffix Concat nodes)
// when their repetition intervals overlap or are adjacent.  Returns
// INVALID_EXPR_ID when the pair cannot be coalesced.
ErrorOr<ExprId> coalesce_repeat_pair(ExprId a, ExprId b, ExprPool &pool) {
  if (a == b)
    return INVALID_EXPR_ID;

  // Scan past matching left Concat prefixes (A · X) | (A · Y) -> A · (X | Y)
  // without allocating; collect prefixes only if the suffixes coalesce.
  const ExprId orig_a = a;
  size_t prefix_depth = 0;
  Expr ae = pool.get(a);
  Expr be = pool.get(b);
  while ((ae.has_repeat || be.has_repeat) && ae.kind == ExprKind::Concat &&
         be.kind == ExprKind::Concat && ae.bin.left == be.bin.left) {
    // Hash-consing guarantees distinct right children when a != b.
    LIBC_ASSERT(ae.bin.right != be.bin.right);
    ++prefix_depth;
    a = ae.bin.right;
    b = be.bin.right;
    ae = pool.get(a);
    be = pool.get(b);
  }
  if (!ae.has_repeat && !be.has_repeat)
    return INVALID_EXPR_ID;

  auto merge_with_suffix = [&](ExprId lhs, ExprId rhs,
                               ExprId suffix) -> ErrorOr<ExprId> {
    // make_concat right-associates, so Concat::left is never itself a Concat.
    LIBC_ASSERT(pool.get(lhs).kind != ExprKind::Concat &&
                pool.get(rhs).kind != ExprKind::Concat);
    ErrorOr<ExprId> left = coalesce_repeat_leaves(lhs, rhs, pool);
    if (!left.has_value() || *left == INVALID_EXPR_ID)
      return left;
    return pool.make_concat(*left, suffix);
  };
  auto merge_with_prefix = [&](ExprId prefix, ExprId lhs,
                               ExprId rhs) -> ErrorOr<ExprId> {
    ErrorOr<ExprId> right = coalesce_repeat_leaves(lhs, rhs, pool);
    if (!right.has_value() || *right == INVALID_EXPR_ID)
      return right;
    return pool.make_concat(prefix, *right);
  };

  // Try coalescing the stripped suffixes across the Concat and leaf shapes:
  //   (X · S) | (Y · S) -> (X | Y) · S  (e.g. a{2,3}·b | a{4,5}·b -> a{2,5}·b)
  //   (X · S) | S       -> (X | ε) · S  (e.g. a{1,3}·b | b -> a{0,3}·b)
  //   (P · X) | P       -> P · (X | ε)  (e.g. b·a{1,3} | b -> b·a{0,3})
  //   X | Y             -> R{m,n}       (direct leaf intervals)
  ErrorOr<ExprId> merged = INVALID_EXPR_ID;
  if (ae.kind == ExprKind::Concat && be.kind == ExprKind::Concat &&
      ae.bin.right == be.bin.right)
    merged = merge_with_suffix(ae.bin.left, be.bin.left, ae.bin.right);
  else if (ae.kind == ExprKind::Concat && ae.bin.right == b)
    merged = merge_with_suffix(ae.bin.left, EMPTY_STR_ID, b);
  else if (be.kind == ExprKind::Concat && be.bin.right == a)
    merged = merge_with_suffix(EMPTY_STR_ID, be.bin.left, a);
  else if (ae.kind == ExprKind::Concat && ae.bin.left == b)
    merged = merge_with_prefix(b, ae.bin.right, EMPTY_STR_ID);
  else if (be.kind == ExprKind::Concat && be.bin.left == a)
    merged = merge_with_prefix(a, EMPTY_STR_ID, be.bin.right);
  else
    merged = coalesce_repeat_leaves(a, b, pool);

  if (!merged.has_value() || *merged == INVALID_EXPR_ID || prefix_depth == 0)
    return merged;

  // Re-wrap the coalesced suffix with the stripped shared Concat prefixes
  // (LIFO iteration): P1 · (P2 · ... · merged).
  ScopedReverseExprIdStack prefixes;
  ExprId cur = orig_a;
  for (size_t i = 0; i < prefix_depth; ++i) {
    const Expr ce = pool.get(cur);
    if (!prefixes.push_back(ce.bin.left))
      return Error(REG_ESPACE);
    cur = ce.bin.right;
  }
  ExprId acc = *merged;
  for (ExprId id : prefixes) {
    ErrorOr<ExprId> next = pool.make_concat(id, acc);
    if (!next.has_value())
      return next;
    acc = *next;
  }
  return acc;
}

// Coalesces leaf Repeat / Star / body / EmptyStr pairs whose repetition
// intervals overlap or are adjacent.  Returns INVALID_EXPR_ID when they
// cannot be coalesced.
ErrorOr<ExprId> coalesce_repeat_leaves(ExprId a, ExprId b, ExprPool &pool) {
  LIBC_ASSERT(a != b);
  const Expr ae = pool.get(a);
  const Expr be = pool.get(b);
  if (ae.kind != ExprKind::Repeat && be.kind != ExprKind::Repeat)
    return INVALID_EXPR_ID;
  // When b is Repeat(a, ...) and a is itself a Repeat, pick body = a so
  // get_interval(a, ae) matches [1, 1] via `id == body` symmetrically with
  // coalesce_repeat_leaves(b, a).
  ExprId body = (ae.kind == ExprKind::Repeat &&
                 !(be.kind == ExprKind::Repeat && be.rep.body == a))
                    ? ae.rep.body
                    : be.rep.body;
  struct RepeatInterval {
    uint16_t min;
    uint16_t max;
  };
  // Undo make_repeat normalizations: R -> [1,1], EmptyStr -> [0,0], and
  // Star(R) -> [0,REPEAT_INF].
  auto get_interval = [&](ExprId id,
                          const Expr e) -> cpp::optional<RepeatInterval> {
    if (e.kind == ExprKind::Repeat && e.rep.body == body)
      return RepeatInterval{e.rep.min, e.rep.max};
    if (e.kind == ExprKind::Star && e.body == body)
      return RepeatInterval{0, REPEAT_INF};
    if (id == body)
      return RepeatInterval{1, 1};
    if (id == EMPTY_STR_ID)
      return RepeatInterval{0, 0};
    return cpp::nullopt;
  };
  cpp::optional<RepeatInterval> lhs = get_interval(a, ae);
  cpp::optional<RepeatInterval> rhs = get_interval(b, be);
  if (!lhs.has_value() || !rhs.has_value())
    return INVALID_EXPR_ID;
  // Integer intervals [lhs->min, lhs->max] and [rhs->min, rhs->max] coalesce
  // iff they overlap or are adjacent (later_min <= earlier_max + 1, e.g.
  // [2,3] U [4,5] = [2,5]).
  uint16_t later_min = cpp::max(lhs->min, rhs->min);
  uint16_t earlier_max = cpp::min(lhs->max, rhs->max);
  if (earlier_max != REPEAT_INF && later_min > earlier_max + 1)
    return INVALID_EXPR_ID;
  return pool.make_repeat(body, cpp::min(lhs->min, rhs->min),
                          cpp::max(lhs->max, rhs->max));
}

// Attempts to coalesce `candidate` into `acc_item`, updating `acc_item` when
// their repetition intervals overlap or are adjacent.
ErrorOr<bool> try_coalesce_into(ExprId &acc_item, ExprId candidate,
                                ExprPool &pool) {
  ErrorOr<ExprId> c = coalesce_repeat_pair(acc_item, candidate, pool);
  if (!c.has_value())
    return Error(c.error());
  if (*c == INVALID_EXPR_ID)
    return false;
  acc_item = *c;
  return true;
}

} // namespace

// Inserts item into a sorted Alt spine without repeat coalescing.
ErrorOr<ExprId> ExprPool::insert_sorted_no_coalesce(ExprId item, ExprId spine) {
  ReverseExprIdStack prefix;
  ExprId cur = spine;
  while (cur != EMPTY_SET_ID) {
    ExprId next = cur;
    const ExprId head = pop_alt_head(next, *this);
    if (item == head)
      return spine;
    if (item < head)
      break;
    if (!prefix.push_back(head))
      return Error(REG_ESPACE);
    cur = next;
  }

  // Splice item at its sorted position and rebuild the skipped prefix.
  ErrorOr<ExprId> tail = prepend_alt_node(item, cur);
  if (!tail.has_value())
    return tail;
  return fold_alt_right(prefix, *tail);
}

// Rebuilds sorted (kept, tail) with item spliced directly into its canonical
// slot so no throwaway intermediate Alt nodes are interned in the pool.
ErrorOr<ExprId> ExprPool::splice_into_sorted_spine(ExprId item,
                                                   ReverseExprIdStack &kept,
                                                   ExprId tail) {
  // Coalescing at least one Repeat yields max >= 1 and never [1, 1], so the
  // merged leaf is Star or Repeat (never EmptyStr or an Alt body) and Concat
  // wrappers never collapse to a bare Alt suffix or prefix.
  LIBC_ASSERT(item != EMPTY_SET_ID && get(item).kind != ExprKind::Alt);

  // Every live entry in kept preceded tail in the sorted spine (id <
  // tail_head), so item >= tail_head iff item belongs in tail.
  if (tail != EMPTY_SET_ID) {
    const Expr te = get(tail);
    const ExprId tail_head = (te.kind == ExprKind::Alt) ? te.bin.left : tail;
    if (item >= tail_head) {
      ErrorOr<ExprId> new_tail = insert_sorted_no_coalesce(item, tail);
      if (!new_tail.has_value())
        return new_tail;
      return fold_alt_right(kept, *new_tail);
    }
  }

  // Otherwise item < head(tail) (or tail is empty); fold kept right-to-left
  // (LIFO) and splice item at its sorted slot.
  ErrorOr<ExprId> acc = tail;
  bool placed = false;
  for (ExprId id : kept) {
    if (id == INVALID_EXPR_ID)
      continue;
    LIBC_ASSERT(id != item);
    if (!placed && item > id) {
      acc = prepend_alt_node(item, *acc);
      if (!acc.has_value())
        return acc;
      placed = true;
    }
    acc = prepend_alt_node(id, *acc);
    if (!acc.has_value())
      return acc;
  }
  return placed ? acc : prepend_alt_node(item, *acc);
}

// Scans a repeat-bearing Alt spine for branches that coalesce with item,
// alternating tail and kept sweeps until fixed point before splicing the result
// back in.  Absorbing a branch in kept can widen acc_item so it coalesces with
// a branch left in tail:
//   a{2,3} | (ε | a | b{1,2}·a{0,3}) -> a{1,3} | ε | b{1,2}·a{0,3}
//                                    -> a{0,3} | b{1,2}·a{0,3}
//                                    -> b{0,2}·a{0,3}
// Each absorption strictly reduces |live(kept)| + |tail|.
ErrorOr<ExprId> ExprPool::coalesce_into_spine(ExprId item, ExprId spine) {
  ReverseExprIdStack kept;
  ExprId acc_item = item;
  ExprId tail = spine;
  bool any_merged = false;
  bool kept_changed = false;
  do {
    // Sweep remaining tail branches, lazily copying skipped heads into kept
    // only when a merge hits so non-coalescing scans allocate nothing.
    ExprId scan = tail;
    while (scan != EMPTY_SET_ID) {
      // Alt::has_repeat summarizes the entire remaining sub-spine.
      if (!get(acc_item).has_repeat && !get(scan).has_repeat)
        break;
      ExprId next_scan = scan;
      const ExprId head = pop_alt_head(next_scan, *this);
      if (!any_merged && head == acc_item)
        return spine;
      ErrorOr<bool> merged = try_coalesce_into(acc_item, head, *this);
      if (!merged.has_value())
        return Error(merged.error());
      if (*merged) {
        while (tail != scan) {
          if (!kept.push_back(pop_alt_head(tail, *this)))
            return Error(REG_ESPACE);
        }
        tail = next_scan;
        scan = next_scan;
        any_merged = true;
        continue;
      }
      scan = next_scan;
    }
    if (!any_merged)
      return insert_sorted_no_coalesce(item, spine);

    // Re-check kept branches in case acc_item widened or changed.
    kept_changed = false;
    for (ExprId &id : kept) {
      if (id == INVALID_EXPR_ID)
        continue;
      if (id == acc_item) {
        // Duplicate absorbed without changing acc_item; no extra sweep needed.
        id = INVALID_EXPR_ID;
        continue;
      }
      ErrorOr<bool> merged = try_coalesce_into(acc_item, id, *this);
      if (!merged.has_value())
        return Error(merged.error());
      if (*merged) {
        id = INVALID_EXPR_ID;
        kept_changed = true;
      }
    }
  } while (kept_changed);

  return splice_into_sorted_spine(acc_item, kept, tail);
}

// Inserts a single non-Alt branch into a sorted Alt spine (or single node),
// coalescing overlapping or adjacent Repeat intervals to a fixed point.
ErrorOr<ExprId> ExprPool::insert_into_spine(ExprId item, ExprId spine) {
  LIBC_ASSERT(item != EMPTY_SET_ID && spine != EMPTY_SET_ID);
  if (item == spine)
    return item;

  const Expr ie = get(item);
  const Expr se = get(spine);
  LIBC_ASSERT(ie.kind != ExprKind::Alt);

  // Base case: combine two non-Alt branches directly.
  if (se.kind != ExprKind::Alt) {
    if (ie.has_repeat || se.has_repeat) {
      ErrorOr<bool> merged = try_coalesce_into(item, spine, *this);
      if (!merged.has_value())
        return Error(merged.error());
      if (*merged)
        return item;
    }
    if (item > spine)
      cpp::swap(item, spine);
    return intern_alt_node(item, spine);
  }

  // Fast path: no Repeat nodes anywhere, so only sorted insertion is needed.
  if (!ie.has_repeat && !se.has_repeat)
    return insert_sorted_no_coalesce(item, spine);

  return coalesce_into_spine(item, spine);
}

ErrorOr<ExprId> ExprPool::make_alt(ExprId l, ExprId r) {
  if (!is_valid_input(l) || !is_valid_input(r))
    return Error(REG_BADPAT);
  // Apply basic algebraic identities for alternation:
  // Ø | R = R | Ø = R (Identity: null set) and R | R = R (Idempotency)
  if (l == EMPTY_SET_ID || r == EMPTY_SET_ID || l == r) {
    if (!ensure_initialized())
      return Error(REG_ESPACE);
    return (l == EMPTY_SET_ID) ? r : l;
  }

  // Single-branch fast paths: insert a non-Alt node into a spine (or leaf).
  const Expr le = get(l);
  const Expr re = get(r);
  if (le.kind != ExprKind::Alt)
    return insert_into_spine(l, r);
  if (re.kind != ExprKind::Alt)
    return insert_into_spine(r, l);

  // Merge two right-associated sorted Alt spines symmetrically in ascending
  // ExprId order so multi-way alternations canonicalize to a single
  // deduplicated tree, preserve make_alt(l, r) == make_alt(r, l), and keep
  // derivative state spaces under '*' and '{m,n}' finite.
  const bool any_repeat = le.has_repeat || re.has_repeat;
  ReverseExprIdStack merged;
  ExprId cur_l = l;
  ExprId cur_r = r;
  while (cur_l != cur_r && cur_l != EMPTY_SET_ID && cur_r != EMPTY_SET_ID) {
    // When Repeat nodes are present, stop as soon as either side reaches its
    // final non-Alt leaf so insert_into_spine(leaf, other_spine) can bridge
    // cross-spine intervals (e.g. a{2,5} bridges a and a* in (a|a*)|(ε|a{2,5})
    // -> ε|a*).  Because both inputs start as Alt spines and Alt::right is
    // never EMPTY_SET_ID, both cur_l and cur_r remain non-empty on exit.
    if (any_repeat &&
        (get(cur_l).kind != ExprKind::Alt || get(cur_r).kind != ExprKind::Alt))
      break;
    ExprId next_l = cur_l;
    ExprId next_r = cur_r;
    const ExprId hl = pop_alt_head(next_l, *this);
    const ExprId hr = pop_alt_head(next_r, *this);
    if (!merged.push_back(cpp::min(hl, hr)))
      return Error(REG_ESPACE);
    if (hl <= hr)
      cur_l = next_l;
    if (hr <= hl)
      cur_r = next_r;
  }

  // Without Repeat nodes, the loop exits only on exhaustion or a shared tail;
  // fold the merged sorted heads onto whichever tail remains:
  // (a | c) | (b | c | d) -> a | b | c | d.
  if (!any_repeat) {
    LIBC_ASSERT(cur_l == cur_r || cur_l == EMPTY_SET_ID ||
                cur_r == EMPTY_SET_ID);
    return fold_alt_right(merged, (cur_l != EMPTY_SET_ID) ? cur_l : cur_r);
  }

  // With Repeat nodes, seed acc with insert_into_spine(leaf, other_spine) and
  // fold merged right-to-left (LIFO) so cross-spine intervals coalesce
  // symmetrically: (a | a*) | (ε | a{2,5}) -> ε | a*.
  LIBC_ASSERT(cur_l != EMPTY_SET_ID && cur_r != EMPTY_SET_ID);
  ErrorOr<ExprId> acc = cur_l;
  if (cur_l != cur_r) {
    acc = (get(cur_l).kind != ExprKind::Alt) ? insert_into_spine(cur_l, cur_r)
                                             : insert_into_spine(cur_r, cur_l);
    if (!acc.has_value())
      return acc;
  }
  for (ExprId id : merged) {
    acc = insert_into_spine(id, *acc);
    if (!acc.has_value())
      return acc;
  }
  return acc;
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL
