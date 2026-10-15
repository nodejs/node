//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Pool for Regular Expression AST nodes (Class Definitions).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_EXPR_POOL_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_EXPR_POOL_H

#include "src/__support/CPP/array.h"
#include "src/__support/CPP/span.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace regex {

/// An arena-based pool for Regular Expression AST nodes.
///
/// This class manages the allocation and hash-consing of Expr nodes. All
/// nodes created through this pool are owned by it and will be freed when
/// the pool is destroyed. Hash-consing ensures that identical expressions
/// are represented by the same ExprId, enabling fast comparison and
/// derivative normalization.
class ExprPool {
  /// Internal storage block for AST nodes.
  ///
  /// Blocks are allocated on demand to avoid large contiguous allocations.
  struct Block {
    /// Number of Expr nodes stored in each block (32 nodes * 16B = 512B).
    static constexpr size_t NODES_PER_BLOCK = 32;
    /// The actual storage for Expr nodes.
    cpp::array<Expr, NODES_PER_BLOCK> nodes{};
  };

  static constexpr size_t NODES_PER_BLOCK = Block::NODES_PER_BLOCK;
  /// Initial directory capacity for 8 blocks (64B), holding up to 256 nodes
  /// before directory growth.
  static constexpr size_t INITIAL_BLOCKS_CAPACITY = 8;
  /// Initial power-of-two hash table size: 64 slots (256B), holding up to 44
  /// unique nodes at 70% max load factor while keeping initial pool footprint
  /// at 512B + 64B + 256B = 832B (< 1 KiB).
  static constexpr size_t INITIAL_HASH_CAPACITY = 64;
  /// Grow the hash table when occupancy reaches 70% to keep linear-probing
  /// chains short.
  static constexpr double MAX_LOAD_FACTOR = 0.7;

  /// The maximum number of nodes allowed in the pool to prevent memory
  /// exhaustion during compilation of highly complex or maliciously crafted
  /// regular expressions.
  static constexpr size_t MAX_NODE_LIMIT = 10000;

  // TODO: Once cpp::unique_ptr and AllocChecker-backed cpp::make_unique (and/or
  // an owning unique_span) are available in src/__support/CPP/, replace raw
  // Block* and span-backed manual new[]/delete[] ownership here so that Block
  // lifecycles, directory growth, and hash-table growth are managed via RAII
  // and ~ExprPool() can be defaulted.

  /// Span of allocated Block* slots (capacity = blocks.size()).
  cpp::span<Block *> blocks;
  size_t block_count = 0;
  /// Total number of nodes allocated across all blocks.
  size_t node_count = 0;

  /// Open-addressing hash table storing ExprIds (0 = empty bucket).
  cpp::span<ExprId> hashtable;

  cpp::span<Block *const> active_blocks() const {
    return blocks.first(block_count);
  }

  /// Maximum number of nodes the current hash table can hold before rehashing.
  size_t max_hashtable_nodes() const {
    return static_cast<size_t>(static_cast<double>(hashtable.size()) *
                               MAX_LOAD_FACTOR);
  }

  /// Single choke point mapping a 1-based ExprId to its 2D block/node slot.
  Expr &slot_at(ExprId id) const {
    LIBC_ASSERT(id != INVALID_EXPR_ID && id <= node_count);
    size_t idx = static_cast<size_t>(id - 1);
    return blocks[idx / NODES_PER_BLOCK]->nodes[idx % NODES_PER_BLOCK];
  }

  ExprId &find_bucket(cpp::span<ExprId> table, const Expr &e) const;
  bool ensure_initialized();
  bool allocate_block();
  bool grow_hashtable();

  /// Core hash-consing function (Interning).
  ///
  /// Guarantees that for any two identical structural definitions of an Expr,
  /// this function will return the same ExprId. This enables O(1) structural
  /// equality via ID comparison.
  ///
  /// \param e A structural definition (proto-node) to intern.
  /// \returns The ExprId of the unique, stable instance in the arena,
  ///          or REG_ESPACE on failure.
  ErrorOr<ExprId> intern(Expr e);

public:
  constexpr ExprPool() = default;
  ~ExprPool();

  /// Resolves an ExprId to a reference.
  const Expr &get(ExprId id) const { return slot_at(id); }

  /// O(1) nullability query for a valid ExprId.
  bool is_nullable(ExprId id) const { return get(id).nullable; }

  /// Returns the current heap bytes allocated by this pool.
  size_t allocated_bytes() const {
    return (block_count * sizeof(Block)) + blocks.size_bytes() +
           hashtable.size_bytes();
  }

  // TODO: Use fluent interface (and_then, transform) for these factories once
  // implemented in ErrorOr / cpp::expected.

  /// Returns an EmptySet node.
  ErrorOr<ExprId> empty_set();
  /// Returns an EmptyStr node.
  ErrorOr<ExprId> empty_str();
  /// Creates or returns an existing Literal node for the given character.
  ErrorOr<ExprId> make_lit(char c);
  /// Normalizing factory for Concatenation (L · R).
  ///
  /// Applies algebraic simplifications before interning:
  /// - (Ø · R) or (R · Ø) => Ø
  /// - (ε · R) or (R · ε) => R
  ErrorOr<ExprId> make_concat(ExprId l, ExprId r);

  /// Normalizing factory for Alternation (L | R).
  ///
  /// Applies algebraic simplifications before interning:
  /// - (Ø | R) or (R | Ø) => R
  /// - (R | R) => R (Idempotency)
  ErrorOr<ExprId> make_alt(ExprId l, ExprId r);
};

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_EXPR_POOL_H
