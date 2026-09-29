// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_TURBOLEV_ESCAPE_ANALYSIS_H_
#define V8_MAGLEV_TURBOLEV_ESCAPE_ANALYSIS_H_

#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-tracer.h"

namespace v8::internal::maglev {

// Design doc:
// https://docs.google.com/document/d/1pUpSxmyUAje8lXQf22cTUTIwGPbZQvIrZ7qPv_QEdDs/edit?usp=sharing

// # Summary
//
// The goal of escape analysis is to remove allocations that don't escape the
// current function (which means that they are not visible outside of the
// current function). For instance, in this function
//
//     function foo() {
//       let obj = { a : 42 };
//       return obj.a;
//     }
//
// `obj` doesn't escape and its definition (allocation+initialization) can thus
// be removed from the graph, after which the function would become a simple
//
//     function foo() {
//       return 42;
//     }
//
//
// # Implementation
//
// Escape Analysis happens in 3 phases:
//
//   * `AnalyzerPrePass` does a single forward pass over the graph to check if
//     there are any objects that might not escape (if not, we stop here) and to
//     mark objects that definitely escape as escaping so that
//     `CandidateAnalyzer` doesn't both tracking them from the start.
//
//   * `CandidateAnalyzer` looks at the graph, computes which objects are
//     escaping, and decides which objects to elide. It also computes the value
//     of every field of elided objects at a given point of the program.
//
//   * `Elider` actually removes the objects from the graph and update the nodes
//     that reference them (in particular Stores and Loads, since those objects
//     are guaranteed to not escape the current function).
//
//
// More implementation details & choices can be found in the design doc (linked
// at the top of the file).

// A candidate starts with status kCanMaybeElide. If we ever find out that it's
// not possible to elide it, it will switch to state kCannotElide.
enum class CandidateStatus { kCanMaybeElide, kCannotElide };

using Candidates = ZoneAbslFlatHashMap<InlinedAllocation*, CandidateStatus>;

struct ObjectField {
  InlinedAllocation* base = nullptr;
  int offset = 0;

  bool operator==(const ObjectField& other) const {
    return base == other.base && offset == other.offset;
  }

  template <typename H>
  friend H AbslHashValue(H h, const ObjectField& mem) {
    return H::combine(std::move(h), mem.base, mem.offset);
  }
};

inline size_t hash_value(ObjectField const& mem) {
  return compiler::turboshaft::fast_hash_combine(mem.base, mem.offset);
}

inline std::ostream& operator<<(std::ostream& os, const ObjectField& mem) {
  return os << mem.base << "[" << mem.offset << "]\n";
}

// TODO(dmercadier): having ObjectField as KeyData in FieldValuesTable is just
// helpful for debugging. Consider removing it once this optimization is stable.
using FieldValuesTable =
    compiler::turboshaft::SnapshotTable<ValueNode*, ObjectField>;
using Key = FieldValuesTable::Key;
using Snapshot = FieldValuesTable::Snapshot;
using MaybeSnapshot = FieldValuesTable::MaybeSnapshot;

struct EscapeAnalysisData {
  EscapeAnalysisData(Graph* graph, MaglevCompilationInfo* compilation_info,
                     Zone* phase_zone)
      : graph(graph),
        compilation_info(compilation_info),
        zone(phase_zone),
        broker(compilation_info->broker()),
        tracer_(compilation_info),
        candidates(phase_zone),
        field_values(phase_zone),
        snapshots(phase_zone),
        keys_mappings(phase_zone),
        loaded_values(phase_zone),
        alloc_dependencies(phase_zone),
        new_phis(phase_zone),
        loop_stack(phase_zone),
        alloc_definition_loop(phase_zone) {
    // We initialize {loop_stack} with a dummy entry so that we don't need
    // special cases when checking if an object is defined within a loop or not.
    loop_stack.push_back({nullptr, false});
  }

  Graph* graph;
  MaglevCompilationInfo* compilation_info;
  Zone* zone;
  compiler::JSHeapBroker* broker;
  Tracer tracer_;

  Candidates candidates;

  FieldValuesTable field_values;

  // Number of predecessor snapshots that were merged to create the current
  // snapshot. This is the predecessor count of the current block, except when
  // visiting a loop header for the 1st time, where the backedge predecessor
  // hasn't been visited yet and thus has no snapshot.
  int merged_predecessor_count = 0;

  ZoneAbslFlatHashMap<NodeBase*, Snapshot> snapshots;
  ZoneAbslFlatHashMap<ObjectField, Key> keys_mappings;

  // In order to handle exception handlers (which have no explicit predecessors
  // in Maglev), we would need to compute the state from the predecessors, but
  // predecessors are not available. For now, we just don't do any escape
  // analysis if the graph has an exception handler.
  // TODO(dmercadier): while iterating the graph, we could record exception
  // handlers predecessors in a side table fairly easily.
  bool has_exception_handler = false;

  // When the CandidateAnalyzer encounters a LoadTaggedField, it must resolve it
  // right away, rather than resolving it once its users are visited, so that if
  // we have a store in between the load and its users, users still see the
  // right value (cf escape-analysis-regress-531021852-1.js for an example).
  ZoneAbslFlatHashMap<LoadTaggedField*, ValueNode*> loaded_values;

  // If there is a key `alloc1` with values `{ alloc2, alloc3 }`, then it means
  // that if `alloc1` cannot be elided, then neither can `alloc2` and `alloc3`.
  ZoneUnorderedMap<InlinedAllocation*, ZoneUnorderedSet<InlinedAllocation*>*>
      alloc_dependencies;

  // {new_phis} is a map from BasicBlock to the new phis that need to be
  // inserted in this basic block. It's populated by CandidateAnalyzer, but phis
  // are only inserted later by the Elider. {new_phis} also stores the Key so
  // that FixLoopPhis can find out the backedge value for the new phis.
  ZoneAbslFlatHashMap<BasicBlock*, ZoneVector<std::pair<Phi*, Key>>*> new_phis;

  // When an object gets marked as escaped inside of a loop, this loop needs to
  // be revisited, as well as the outer loops. Unless the object was defined
  // inside of the current loop: in that case it wouldn't have any loop phi and
  // thus marking it as escaping won't invalidate anything that was computed
  // earlier. Here is how we track which loops need to be revisited during the
  // analysis phase:
  //
  //   1. When we enter a loop, we push it to {loop_stack}.
  //
  //   2. When we encounter an InlinedAllocation, we look in {loop_stack} what
  //      the current loop is, and we record it in {alloc_definition_loop}.
  //
  //   3. When we escape an InlinedAllocation, we set the
  //      `has_escaped_candidate` field of all loops in {loop_stack} to true,
  //      until we reach its defining loop (which won't need revisit).
  //
  //   4. When we reach a loop backedge, we pop it from {loop_stack} and check
  //      if it needs revisit based on its `has_escaped_candidate` field.
  struct LoopHeader {
    BasicBlock* header;
    bool has_escaped_candidate;
  };
  ZoneVector<LoopHeader> loop_stack;
  ZoneAbslFlatHashMap<InlinedAllocation*, BasicBlock*> alloc_definition_loop;

  MaglevGraphLabeller* labeller() {
    DCHECK(compilation_info->has_graph_labeller());
    return compilation_info->graph_labeller();
  }

  ValueNode* ResolveBase(Input input, int predecessor_index = -1) {
    return ResolveBase(input.node(), predecessor_index);
  }
  ValueNode* ResolveBase(ValueNode* node, int predecessor_index = -1);

  ValueNode* ResolveLoadBase(Input base_input, int offset, ValueNode* fallback,
                             int predecessor_index = -1) {
    return ResolveLoadBase(base_input.node(), offset, fallback,
                           predecessor_index);
  }
  ValueNode* ResolveLoadBase(ValueNode* base, int offset, ValueNode* fallback,
                             int predecessor_index = -1);

  Key TryGetKeyFor(ObjectField addr);

  InlinedAllocation* TryGetCandidateInlinedAllocation(
      Input input, int predecessor_index = -1) {
    return TryGetCandidateInlinedAllocation(input.node(), predecessor_index);
  }
  InlinedAllocation* TryGetCandidateInlinedAllocation(
      ValueNode* node, int predecessor_index = -1);

  base::SmallVector<compiler::MapRef, 8> GetMaps(InlinedAllocation* alloc);

  bool HasMapFor(InlinedAllocation* alloc) {
    DCHECK_EQ(alloc, TryGetCandidateInlinedAllocation(alloc));
    ObjectField addr = ObjectField{alloc, 0};
    Key key = TryGetKeyFor(addr);
    DCHECK(key.valid());
    return field_values.Get(key) != nullptr;
  }

  // {value} is being stored into {base}. As a result, if {base} is not
  // escaped, then {value} cannot be escaped either.
  void RecordDependentAllocation(InlinedAllocation* base,
                                 InlinedAllocation* value);

  void MarkAsEscapedIfCandidate(ValueNode* node, int predecessor_index = -1);

  void MarkAsEscaped(InlinedAllocation* alloc);

  bool HasEscaped(InlinedAllocation* alloc);

  ValueNode* Get(InlinedAllocation* base, int field);
  Key GetOrCreateKey(InlinedAllocation* base, int field);
};

class EscapeAnalysis {
 public:
  EscapeAnalysis(Graph* graph, MaglevCompilationInfo* compilation_info,
                 Zone* phase_zone)
      : data_(graph, compilation_info, phase_zone), tracer_(compilation_info) {}

  static void Run(Graph* graph, MaglevCompilationInfo* compilation_info,
                  Zone* phase_zone);

 private:
  // Iterate the graph forward, tracking every field of each candidate to
  // determine if they can be elided or not.
  void AnalyzeCandidates();

  // Actually does the eliding of objects that can be escape-analyzed away.
  void ElideCandidates();

  // Returns true if any elidable candidate is recorded in {data_}.
  bool HasElidableCandidates() const;

  MaglevGraphLabeller* labeller() { return data_.labeller(); }

  EscapeAnalysisData data_;
  Tracer tracer_;
};

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_TURBOLEV_ESCAPE_ANALYSIS_H_
