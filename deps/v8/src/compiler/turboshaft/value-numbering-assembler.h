// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_VALUE_NUMBERING_ASSEMBLER_H_
#define V8_COMPILER_TURBOSHAFT_VALUE_NUMBERING_ASSEMBLER_H_

#include "src/base/logging.h"
#include "src/base/vector.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/fast-hash.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/utils/utils.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace turboshaft {

// Value numbering removes redundant nodes from the graph. A simple example
// could be:
//
//   x = a + b
//   y = a + b
//   z = x * y
//
// Is simplified to
//
//   x = a + b
//   z = x * x
//
// It works by storing previously seen nodes in a hashmap, and when visiting a
// new node, we check to see if it's already in the hashmap. If yes, then we
// return the old node. If not, then we keep the new one (and add it into the
// hashmap). A high-level pseudo-code would be:
//
//   def VisitOp(op):
//     if op in hashmap:
//        return hashmap.get(op)
//     else:
//        hashmap.add(op)
//        return op
//
// We implemented our own hashmap (to have more control, it should become
// clearer why by the end of this explanation). When there is a collision, we
// look at the next index (and the next one if there is yet another collision,
// etc). While not the fastest approach, it has the advantage of not requiring
// any dynamic memory allocation (besides the initial table, and the resizing).
//
// For the approach describe above (the pseudocode and the paragraph before it)
// to be correct, a node should only be replaced by a node defined in blocks
// that dominate the current block. Thus, this assembler should only be used
// with OptimizationPhases that iterate the graph in VisitOrder::kDominator
// order. Then, when going down the dominator tree, we add nodes to the hashmap,
// and when going back up the dominator tree, we remove nodes from the hashmap.
//
// In order to efficiently remove all the nodes of a given block from the
// hashmap, we maintain a linked-list of hashmap entries per block (this way, we
// don't have to iterate the wole hashmap). Note that, in practice, we think in
// terms of "depth" rather than "block", and we thus have one linked-list per
// depth of the dominator tree. The heads of those linked lists are stored in
// the vector {depths_heads_}. The linked lists are then implemented in-place in
// the hashtable entries, thanks to the `depth_neighboring_entry` field of the
// `Entry` structure.
// To remove all of the entries from a given linked list, we iterate the entries
// in the linked list, setting all of their `hash` field to 0 (we prevent hashes
// from being equal to 0, in order to detect empty entries: their hash is 0).

class ValueNumberingAssembler : public Assembler {
  // ValueNumberingAssembler inherits directly from Assembler because it
  // overwrites the last operation in case of a cache hit, which assumes that
  // the base assembler emits everything exactly as given without applying any
  // optimizations.
  using Base = Assembler;

 public:
  ValueNumberingAssembler(Graph* graph, Zone* phase_zone)
      : Assembler(graph, phase_zone), depths_heads_(phase_zone) {
    table_ = phase_zone->NewVector<Entry>(
        base::bits::RoundUpToPowerOfTwo(
            std::max<size_t>(128, graph->op_id_capacity() / 2)),
        Entry());
    entry_count_ = 0;
    mask_ = table_.size() - 1;
    current_depth_ = -1;
  }

#define EMIT_OP(Name)                                    \
  template <class... Args>                               \
  OpIndex Name(Args... args) {                           \
    OpIndex next_index = graph().next_operation_index(); \
    USE(next_index);                                     \
    OpIndex result = Base::Name(args...);                \
    DCHECK_EQ(next_index, result);                       \
    return AddOrFind<Name##Op>(result);                  \
  }
  TURBOSHAFT_OPERATION_LIST(EMIT_OP)
#undef EMIT_OP

  void EnterBlock(const Block& block) {
    int new_depth = block.Depth();
    // Remember that this assembler should only be used for OptimizationPhases
    // that visit the graph in VisitOrder::kDominator order. We can't properly
    // check that here, but we do two checks, which should be enough to ensure
    // that we are actually visiting the graph in dominator order:
    //  - There should be only one block at depth 0 (the root).
    //  - There should be no "jumps" downward in the dominator tree ({new_depth}
    //    cannot be lower than {current_depth}+1).
    DCHECK_IMPLIES(current_depth_ == 0, new_depth != 0);
    DCHECK_LE(new_depth, current_depth_ + 1);
    if (new_depth <= current_depth_) {
      while (current_depth_ >= new_depth) {
        ClearCurrentDepthEntries();
        --current_depth_;
      }
    }
    current_depth_ = new_depth;
    depths_heads_.push_back(nullptr);
  }

 private:
  // TODO(dmercadier): Once the mapping from Operations to Blocks has been added
  // to turboshaft, remove the `block` field from the `Entry` structure.
  struct Entry {
    OpIndex value;
    BlockIndex block;
    size_t hash = 0;
    Entry* depth_neighboring_entry = nullptr;
  };

  template <class Op>
  OpIndex AddOrFind(OpIndex op_idx) {
    if constexpr (!Op::properties.can_be_eliminated ||
                  std::is_same<Op, PendingLoopPhiOp>::value) {
      return op_idx;
    }
    RehashIfNeeded();

    const Op& op = graph().Get(op_idx).Cast<Op>();
    constexpr bool same_block_only = std::is_same<Op, PhiOp>::value;
    size_t hash = ComputeHash<same_block_only>(op);
    size_t start_index = hash & mask_;
    for (size_t i = start_index;; i = NextEntryIndex(i)) {
      Entry& entry = table_[i];
      if (entry.hash == 0) {
        // We didn't find {op} in {table_}. Inserting it and returning.
        table_[i] =
            Entry{op_idx, current_block()->index(), hash, depths_heads_.back()};
        depths_heads_.back() = &table_[i];
        ++entry_count_;
        return op_idx;
      }
      if (entry.hash == hash) {
        const Operation& entry_op = graph().Get(entry.value);
        if (entry_op.Is<Op>() &&
            (!same_block_only || entry.block == current_block()->index()) &&
            entry_op.Cast<Op>() == op) {
          graph().RemoveLast();
          return entry.value;
        }
      }
      // Making sure that we don't have an infinite loop.
      DCHECK_NE(start_index, NextEntryIndex(i));
    }
  }

  // Remove all of the Entries of the current depth.
  void ClearCurrentDepthEntries() {
    for (Entry* entry = depths_heads_.back(); entry != nullptr;) {
      entry->hash = 0;
      Entry* next_entry = entry->depth_neighboring_entry;
      entry->depth_neighboring_entry = nullptr;
      entry = next_entry;
      --entry_count_;
    }
    depths_heads_.pop_back();
  }

  // If the table is too full, double its size and re-insert the old entries.
  void RehashIfNeeded() {
    if (V8_LIKELY(table_.size() - (table_.size() / 4) > entry_count_)) return;
    base::Vector<Entry> new_table = table_ =
        phase_zone()->NewVector<Entry>(table_.size() * 2, Entry());
    size_t mask = mask_ = table_.size() - 1;

    for (size_t depth_idx = 0; depth_idx < depths_heads_.size(); depth_idx++) {
      // It's important to fill the new hash by inserting data in increasing
      // depth order, in order to avoid holes when later calling
      // ClearCurrentDepthEntries. Consider for instance:
      //
      //  ---+------+------+------+----
      //     |  a1  |  a2  |  a3  |
      //  ---+------+------+------+----
      //
      // Where a1, a2 and a3 have the same hash. By construction, we know that
      // depth(a1) <= depth(a2) <= depth(a3). If, when re-hashing, we were to
      // insert them in another order, say:
      //
      //  ---+------+------+------+----
      //     |  a3  |  a1  |  a2  |
      //  ---+------+------+------+----
      //
      // Then, when we'll call ClearCurrentDepthEntries to remove entries from
      // a3's depth, we'll get this:
      //
      //  ---+------+------+------+----
      //     | null |  a1  |  a2  |
      //  ---+------+------+------+----
      //
      // And, when looking if a1 is in the hash, we'd find a "null" where we
      // expect it, and assume that it's not present. If, instead, we always
      // conserve the increasing depth order, then when removing a3, we'd get:
      //
      //  ---+------+------+------+----
      //     |  a1  |  a2  | null |
      //  ---+------+------+------+----
      //
      // Where we can still find a1 and a2.
      Entry* entry = depths_heads_[depth_idx];
      depths_heads_[depth_idx] = nullptr;

      while (entry != nullptr) {
        for (size_t i = entry->hash & mask;; i = NextEntryIndex(i)) {
          if (new_table[i].hash == 0) {
            new_table[i] = *entry;
            Entry* next_entry = entry->depth_neighboring_entry;
            new_table[i].depth_neighboring_entry = depths_heads_[depth_idx];
            depths_heads_[depth_idx] = &new_table[i];
            entry = next_entry;
            break;
          }
        }
      }
    }
  }

  template <bool same_block_only, class Op>
  size_t ComputeHash(const Op& op) {
    size_t hash = op.hash_value();
    if (same_block_only) {
      hash = fast_hash_combine(current_block()->index(), hash);
    }
    if (V8_UNLIKELY(hash == 0)) return 1;
    return hash;
  }

  size_t NextEntryIndex(size_t index) { return (index + 1) & mask_; }
  Entry* NextEntry(Entry* entry) {
    return V8_LIKELY(entry + 1 < table_.end()) ? entry + 1 : &table_[0];
  }
  Entry* PrevEntry(Entry* entry) {
    return V8_LIKELY(entry > table_.begin()) ? entry - 1 : table_.end() - 1;
  }

  int current_depth_;
  base::Vector<Entry> table_;
  size_t mask_;
  size_t entry_count_;
  ZoneVector<Entry*> depths_heads_;
};

}  // namespace turboshaft
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TURBOSHAFT_VALUE_NUMBERING_ASSEMBLER_H_
