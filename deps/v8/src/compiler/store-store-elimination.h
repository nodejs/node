// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_STORE_STORE_ELIMINATION_H_
#define V8_COMPILER_STORE_STORE_ELIMINATION_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/simplified-operator.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class TickCounter;

namespace compiler {

// Store-store elimination.
//
// The aim of this optimization is to detect the following pattern in the
// effect graph:
//
// - StoreField[+24, kRepTagged](263, ...)
//
//   ... lots of nodes from which the field at offset 24 of the object
//       returned by node #263 cannot be observed ...
//
// - StoreField[+24, kRepTagged](263, ...)
//
// In such situations, the earlier StoreField cannot be observed, and can be
// eliminated. This optimization should work for any offset and input node, of
// course.
//
// The optimization also works across splits. It currently does not work for
// loops, because we tend to put a stack check in loops, and like deopts,
// stack checks can observe anything.

// Assumption: every byte of a JS object is only ever accessed through one
// offset. For instance, byte 15 of a given object may be accessed using a
// two-byte read at offset 14, or a four-byte read at offset 12, but never
// both in the same program.
//
// This implementation needs all dead nodes removed from the graph, and the
// graph should be trimmed.
class StoreStoreElimination final {
 public:
  static void Run(JSGraph* js_graph, TickCounter* tick_counter,
                  Zone* temp_zone);

 private:
  using StoreOffset = uint32_t;

  struct UnobservableStore {
    NodeId id_;
    StoreOffset offset_;

    bool operator==(const UnobservableStore other) const {
      return (id_ == other.id_) && (offset_ == other.offset_);
    }

    bool operator<(const UnobservableStore other) const {
      return (id_ < other.id_) || (id_ == other.id_ && offset_ < other.offset_);
    }
  };

  // Instances of UnobservablesSet are immutable. They represent either a set of
  // UnobservableStores, or the "unvisited empty set".
  //
  // We apply some sharing to save memory. The class UnobservablesSet is only a
  // pointer wide, and a copy does not use any heap (or temp_zone) memory. Most
  // changes to an UnobservablesSet might allocate in the temp_zone.
  //
  // The size of an instance should be the size of a pointer, plus additional
  // space in the zone in the case of non-unvisited UnobservablesSets. Copying
  // an UnobservablesSet allocates no memory.
  class UnobservablesSet final {
   public:
    // Creates a new UnobservablesSet, with the null set.
    static UnobservablesSet Unvisited() { return UnobservablesSet(); }

    // Create a new empty UnobservablesSet. This allocates in the zone, and
    // can probably be optimized to use a global singleton.
    static UnobservablesSet VisitedEmpty(Zone* zone);
    UnobservablesSet(const UnobservablesSet& other) V8_NOEXCEPT = default;

    // Computes the intersection of two UnobservablesSets. If one of the sets is
    // empty, will return empty.
    UnobservablesSet Intersect(const UnobservablesSet& other,
                               const UnobservablesSet& empty, Zone* zone) const;

    // Returns a set that it is the current one, plus the observation obs passed
    // as parameter. If said obs it's already in the set, we don't have to
    // create a new one.
    UnobservablesSet Add(UnobservableStore obs, Zone* zone) const;

    // Returns a set that it is the current one, except for all of the
    // observations with offset off. This is done by creating a new set and
    // copying all observations with different offsets.
    // This can probably be done better if the observations are stored first by
    // offset and then by node.
    // We are removing all nodes with offset off since different nodes may
    // alias one another, and we currently we don't have the means to know if
    // two nodes are definitely the same value.
    UnobservablesSet RemoveSameOffset(StoreOffset off, Zone* zone) const;

    const ZoneSet<UnobservableStore>* set() const { return set_; }

    bool IsUnvisited() const { return set_ == nullptr; }
    bool IsEmpty() const { return set_ == nullptr || set_->empty(); }
    bool Contains(UnobservableStore obs) const {
      return set_ != nullptr && (set_->find(obs) != set_->end());
    }

    bool operator==(const UnobservablesSet& other) const {
      if (IsUnvisited() || other.IsUnvisited()) {
        return IsEmpty() && other.IsEmpty();
      } else {
        // Both pointers guaranteed not to be nullptrs.
        return *set() == *(other.set());
      }
    }

    bool operator!=(const UnobservablesSet& other) const {
      return !(*this == other);
    }

   private:
    UnobservablesSet();
    explicit UnobservablesSet(const ZoneSet<UnobservableStore>* set)
        : set_(set) {}
    const ZoneSet<UnobservableStore>* set_;
  };

  class RedundantStoreFinder final {
   public:
    // Note that we Initialize unobservable_ with js_graph->graph->NodeCount()
    // amount of empty sets.
    RedundantStoreFinder(JSGraph* js_graph, TickCounter* tick_counter,
                         Zone* temp_zone)
        : jsgraph_(js_graph),
          tick_counter_(tick_counter),
          temp_zone_(temp_zone),
          revisit_(temp_zone),
          in_revisit_(js_graph->graph()->NodeCount(), temp_zone),
          unobservable_(js_graph->graph()->NodeCount(),
                        StoreStoreElimination::UnobservablesSet::Unvisited(),
                        temp_zone),
          to_remove_(temp_zone),
          unobservables_visited_empty_(
              StoreStoreElimination::UnobservablesSet::VisitedEmpty(
                  temp_zone)) {}

    // Crawls from the end of the graph to the beginning, with the objective of
    // finding redundant stores.
    void Find();

    // This method is used for const correctness to go through the final list of
    // redundant stores that are replaced on the graph.
    const ZoneSet<Node*>& to_remove_const() { return to_remove_; }

   private:
    // Assumption: All effectful nodes are reachable from End via a sequence of
    // control, then a sequence of effect edges.
    // Visit goes through the control chain, visiting effectful nodes that it
    // encounters.
    void Visit(Node* node);

    // Marks effect inputs for visiting, if we are able to update this path of
    // the graph.
    void VisitEffectfulNode(Node* node);

    // Compute the intersection of the UnobservablesSets of all effect uses and
    // return it.
    // The result UnobservablesSet will never be null.
    UnobservablesSet RecomputeUseIntersection(Node* node);

    // Recompute unobservables-set for a node. Will also mark superfluous nodes
    // as to be removed.
    UnobservablesSet RecomputeSet(Node* node, const UnobservablesSet& uses);

    // Returns true if node's opcode cannot observe StoreFields.
    static bool CannotObserveStoreField(Node* node);

    void MarkForRevisit(Node* node);
    bool HasBeenVisited(Node* node);

    // To safely cast an offset from a FieldAccess, which has a potentially
    // wider range (namely int).
    StoreOffset ToOffset(const FieldAccess& access) {
      DCHECK_GE(access.offset, 0);
      return static_cast<StoreOffset>(access.offset);
    }

    JSGraph* jsgraph() const { return jsgraph_; }
    Isolate* isolate() { return jsgraph()->isolate(); }
    Zone* temp_zone() const { return temp_zone_; }
    UnobservablesSet& unobservable_for_id(NodeId id) {
      DCHECK_LT(id, unobservable_.size());
      return unobservable_[id];
    }
    ZoneSet<Node*>& to_remove() { return to_remove_; }

    JSGraph* const jsgraph_;
    TickCounter* const tick_counter_;
    Zone* const temp_zone_;

    ZoneStack<Node*> revisit_;
    ZoneVector<bool> in_revisit_;

    // Maps node IDs to UnobservableNodeSets.
    ZoneVector<UnobservablesSet> unobservable_;
    ZoneSet<Node*> to_remove_;
    const UnobservablesSet unobservables_visited_empty_;
  };
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_STORE_STORE_ELIMINATION_H_
