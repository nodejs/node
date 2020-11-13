// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/store-store-elimination.h"

#include "src/codegen/tick-counter.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/persistent-map.h"
#include "src/compiler/simplified-operator.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(fmt, ...)                                         \
  do {                                                          \
    if (FLAG_trace_store_elimination) {                         \
      PrintF("RedundantStoreFinder: " fmt "\n", ##__VA_ARGS__); \
    }                                                           \
  } while (false)

// CHECK_EXTRA is like CHECK, but has two or more arguments: a boolean
// expression, a format string, and any number of extra arguments. The boolean
// expression will be evaluated at runtime. If it evaluates to false, then an
// error message will be shown containing the condition, as well as the extra
// info formatted like with printf.
#define CHECK_EXTRA(condition, fmt, ...)                                      \
  do {                                                                        \
    if (V8_UNLIKELY(!(condition))) {                                          \
      FATAL("Check failed: %s. Extra info: " fmt, #condition, ##__VA_ARGS__); \
    }                                                                         \
  } while (false)

#ifdef DEBUG
#define DCHECK_EXTRA(condition, fmt, ...) \
  CHECK_EXTRA(condition, fmt, ##__VA_ARGS__)
#else
#define DCHECK_EXTRA(condition, fmt, ...) ((void)0)
#endif

namespace {

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

size_t hash_value(const UnobservableStore& p) {
  return base::hash_combine(p.id_, p.offset_);
}

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
 private:
  using KeyT = UnobservableStore;
  using ValueT = bool;  // Emulates set semantics in the map.

  // The PersistentMap uses a special value to signify 'not present'. We use
  // a boolean value to emulate set semantics.
  static constexpr ValueT kNotPresent = false;
  static constexpr ValueT kPresent = true;

 public:
  using SetT = PersistentMap<KeyT, ValueT>;

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

  const SetT* set() const { return set_; }

  bool IsUnvisited() const { return set_ == nullptr; }
  bool IsEmpty() const {
    return set_ == nullptr || set_->begin() == set_->end();
  }
  bool Contains(UnobservableStore obs) const {
    return set_ != nullptr && set_->Get(obs) != kNotPresent;
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
  UnobservablesSet() = default;
  explicit UnobservablesSet(const SetT* set) : set_(set) {}

  static SetT* NewSet(Zone* zone) {
    return zone->New<UnobservablesSet::SetT>(zone, kNotPresent);
  }

  static void SetAdd(SetT* set, const KeyT& key) { set->Set(key, kPresent); }
  static void SetErase(SetT* set, const KeyT& key) {
    set->Set(key, kNotPresent);
  }

  const SetT* set_ = nullptr;
};

// These definitions are here in order to please the linker, which in debug mode
// sometimes requires static constants to be defined in .cc files.
constexpr UnobservablesSet::ValueT UnobservablesSet::kNotPresent;
constexpr UnobservablesSet::ValueT UnobservablesSet::kPresent;

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
                      UnobservablesSet::Unvisited(), temp_zone),
        to_remove_(temp_zone),
        unobservables_visited_empty_(
            UnobservablesSet::VisitedEmpty(temp_zone)) {}

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

void RedundantStoreFinder::Find() {
  Visit(jsgraph()->graph()->end());

  while (!revisit_.empty()) {
    tick_counter_->TickAndMaybeEnterSafepoint();
    Node* next = revisit_.top();
    revisit_.pop();
    DCHECK_LT(next->id(), in_revisit_.size());
    in_revisit_[next->id()] = false;
    Visit(next);
  }

#ifdef DEBUG
  // Check that we visited all the StoreFields
  AllNodes all(temp_zone(), jsgraph()->graph());
  for (Node* node : all.reachable) {
    if (node->op()->opcode() == IrOpcode::kStoreField) {
      DCHECK_EXTRA(HasBeenVisited(node), "#%d:%s", node->id(),
                   node->op()->mnemonic());
    }
  }
#endif
}

void RedundantStoreFinder::MarkForRevisit(Node* node) {
  DCHECK_LT(node->id(), in_revisit_.size());
  if (!in_revisit_[node->id()]) {
    revisit_.push(node);
    in_revisit_[node->id()] = true;
  }
}

bool RedundantStoreFinder::HasBeenVisited(Node* node) {
  return !unobservable_for_id(node->id()).IsUnvisited();
}

UnobservablesSet RedundantStoreFinder::RecomputeSet(
    Node* node, const UnobservablesSet& uses) {
  switch (node->op()->opcode()) {
    case IrOpcode::kStoreField: {
      Node* stored_to = node->InputAt(0);
      const FieldAccess& access = FieldAccessOf(node->op());
      StoreOffset offset = ToOffset(access);

      UnobservableStore observation = {stored_to->id(), offset};
      bool is_not_observable = uses.Contains(observation);

      if (is_not_observable) {
        TRACE("  #%d is StoreField[+%d,%s](#%d), unobservable", node->id(),
              offset, MachineReprToString(access.machine_type.representation()),
              stored_to->id());
        to_remove().insert(node);
        return uses;
      } else {
        TRACE("  #%d is StoreField[+%d,%s](#%d), observable, recording in set",
              node->id(), offset,
              MachineReprToString(access.machine_type.representation()),
              stored_to->id());
        return uses.Add(observation, temp_zone());
      }
    }
    case IrOpcode::kLoadField: {
      Node* loaded_from = node->InputAt(0);
      const FieldAccess& access = FieldAccessOf(node->op());
      StoreOffset offset = ToOffset(access);

      TRACE(
          "  #%d is LoadField[+%d,%s](#%d), removing all offsets [+%d] from "
          "set",
          node->id(), offset,
          MachineReprToString(access.machine_type.representation()),
          loaded_from->id(), offset);

      return uses.RemoveSameOffset(offset, temp_zone());
    }
    default:
      if (CannotObserveStoreField(node)) {
        TRACE("  #%d:%s can observe nothing, set stays unchanged", node->id(),
              node->op()->mnemonic());
        return uses;
      } else {
        TRACE("  #%d:%s might observe anything, recording empty set",
              node->id(), node->op()->mnemonic());
        return unobservables_visited_empty_;
      }
  }
  UNREACHABLE();
}

bool RedundantStoreFinder::CannotObserveStoreField(Node* node) {
  IrOpcode::Value opcode = node->opcode();
  return opcode == IrOpcode::kLoadElement || opcode == IrOpcode::kLoad ||
         opcode == IrOpcode::kStore || opcode == IrOpcode::kEffectPhi ||
         opcode == IrOpcode::kStoreElement ||
         opcode == IrOpcode::kUnsafePointerAdd || opcode == IrOpcode::kRetain;
}

void RedundantStoreFinder::Visit(Node* node) {
  if (!HasBeenVisited(node)) {
    for (int i = 0; i < node->op()->ControlInputCount(); i++) {
      Node* control_input = NodeProperties::GetControlInput(node, i);
      if (!HasBeenVisited(control_input)) {
        MarkForRevisit(control_input);
      }
    }
  }

  bool is_effectful = node->op()->EffectInputCount() >= 1;
  if (is_effectful) {
    // mark all effect inputs for revisiting (if they might have stale state).
    VisitEffectfulNode(node);
    DCHECK(HasBeenVisited(node));
  } else if (!HasBeenVisited(node)) {
    // Mark as visited.
    unobservable_for_id(node->id()) = unobservables_visited_empty_;
  }
}

void RedundantStoreFinder::VisitEffectfulNode(Node* node) {
  if (HasBeenVisited(node)) {
    TRACE("- Revisiting: #%d:%s", node->id(), node->op()->mnemonic());
  }
  UnobservablesSet after_set = RecomputeUseIntersection(node);
  UnobservablesSet before_set = RecomputeSet(node, after_set);
  DCHECK(!before_set.IsUnvisited());

  UnobservablesSet stores_for_node = unobservable_for_id(node->id());
  bool cur_set_changed =
      stores_for_node.IsUnvisited() || stores_for_node != before_set;
  if (!cur_set_changed) {
    // We will not be able to update the part of this chain above any more.
    // Exit.
    TRACE("+ No change: stabilized. Not visiting effect inputs.");
  } else {
    unobservable_for_id(node->id()) = before_set;

    // Mark effect inputs for visiting.
    for (int i = 0; i < node->op()->EffectInputCount(); i++) {
      Node* input = NodeProperties::GetEffectInput(node, i);
      TRACE("    marking #%d:%s for revisit", input->id(),
            input->op()->mnemonic());
      MarkForRevisit(input);
    }
  }
}

UnobservablesSet RedundantStoreFinder::RecomputeUseIntersection(Node* node) {
  // There were no effect uses. Break early.
  if (node->op()->EffectOutputCount() == 0) {
    IrOpcode::Value opcode = node->opcode();
    // List of opcodes that may end this effect chain. The opcodes are not
    // important to the soundness of this optimization; this serves as a
    // general check. Add opcodes to this list as it suits you.
    //
    // Everything is observable after these opcodes; return the empty set.
    DCHECK_EXTRA(
        opcode == IrOpcode::kReturn || opcode == IrOpcode::kTerminate ||
            opcode == IrOpcode::kDeoptimize || opcode == IrOpcode::kThrow ||
            opcode == IrOpcode::kTailCall,
        "for #%d:%s", node->id(), node->op()->mnemonic());
    USE(opcode);

    return unobservables_visited_empty_;
  }

  // {first} == true indicates that we haven't looked at any elements yet.
  // {first} == false indicates that cur_set is the intersection of at least one
  // thing.
  bool first = true;
  UnobservablesSet cur_set = UnobservablesSet::Unvisited();  // irrelevant
  for (Edge edge : node->use_edges()) {
    if (!NodeProperties::IsEffectEdge(edge)) {
      continue;
    }

    // Intersect with the new use node.
    Node* use = edge.from();
    UnobservablesSet new_set = unobservable_for_id(use->id());
    if (first) {
      first = false;
      cur_set = new_set;
      if (cur_set.IsUnvisited()) {
        cur_set = unobservables_visited_empty_;
      }
    } else {
      cur_set =
          cur_set.Intersect(new_set, unobservables_visited_empty_, temp_zone());
    }

    // Break fast for the empty set since the intersection will always be empty.
    if (cur_set.IsEmpty()) {
      break;
    }
  }

  DCHECK(!cur_set.IsUnvisited());
  return cur_set;
}

UnobservablesSet UnobservablesSet::VisitedEmpty(Zone* zone) {
  return UnobservablesSet(NewSet(zone));
}

UnobservablesSet UnobservablesSet::Intersect(const UnobservablesSet& other,
                                             const UnobservablesSet& empty,
                                             Zone* zone) const {
  if (IsEmpty() || other.IsEmpty()) return empty;

  UnobservablesSet::SetT* intersection = NewSet(zone);
  for (const auto& triple : set()->Zip(*other.set())) {
    if (std::get<1>(triple) && std::get<2>(triple)) {
      intersection->Set(std::get<0>(triple), kPresent);
    }
  }

  return UnobservablesSet(intersection);
}

UnobservablesSet UnobservablesSet::Add(UnobservableStore obs,
                                       Zone* zone) const {
  if (set()->Get(obs) != kNotPresent) return *this;

  UnobservablesSet::SetT* new_set = NewSet(zone);
  *new_set = *set();
  SetAdd(new_set, obs);

  return UnobservablesSet(new_set);
}

UnobservablesSet UnobservablesSet::RemoveSameOffset(StoreOffset offset,
                                                    Zone* zone) const {
  UnobservablesSet::SetT* new_set = NewSet(zone);
  *new_set = *set();

  // Remove elements with the given offset.
  for (const auto& entry : *new_set) {
    const UnobservableStore& obs = entry.first;
    if (obs.offset_ == offset) SetErase(new_set, obs);
  }

  return UnobservablesSet(new_set);
}

}  // namespace

// static
void StoreStoreElimination::Run(JSGraph* js_graph, TickCounter* tick_counter,
                                Zone* temp_zone) {
  // Find superfluous nodes
  RedundantStoreFinder finder(js_graph, tick_counter, temp_zone);
  finder.Find();

  // Remove superfluous nodes
  for (Node* node : finder.to_remove_const()) {
    if (FLAG_trace_store_elimination) {
      PrintF("StoreStoreElimination::Run: Eliminating node #%d:%s\n",
             node->id(), node->op()->mnemonic());
    }
    Node* previous_effect = NodeProperties::GetEffectInput(node);
    NodeProperties::ReplaceUses(node, nullptr, previous_effect, nullptr,
                                nullptr);
    node->Kill();
  }
}

#undef TRACE
#undef CHECK_EXTRA
#undef DCHECK_EXTRA

}  // namespace compiler
}  // namespace internal
}  // namespace v8
