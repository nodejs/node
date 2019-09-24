// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

#include "src/compiler/store-store-elimination.h"

#include "src/codegen/tick-counter.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"

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

void StoreStoreElimination::RedundantStoreFinder::Find() {
  Visit(jsgraph()->graph()->end());

  while (!revisit_.empty()) {
    tick_counter_->DoTick();
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

void StoreStoreElimination::RedundantStoreFinder::MarkForRevisit(Node* node) {
  DCHECK_LT(node->id(), in_revisit_.size());
  if (!in_revisit_[node->id()]) {
    revisit_.push(node);
    in_revisit_[node->id()] = true;
  }
}

bool StoreStoreElimination::RedundantStoreFinder::HasBeenVisited(Node* node) {
  return !unobservable_for_id(node->id()).IsUnvisited();
}

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

StoreStoreElimination::UnobservablesSet
StoreStoreElimination::RedundantStoreFinder::RecomputeSet(
    Node* node, const StoreStoreElimination::UnobservablesSet& uses) {
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

bool StoreStoreElimination::RedundantStoreFinder::CannotObserveStoreField(
    Node* node) {
  IrOpcode::Value opcode = node->opcode();
  return opcode == IrOpcode::kLoadElement || opcode == IrOpcode::kLoad ||
         opcode == IrOpcode::kStore || opcode == IrOpcode::kEffectPhi ||
         opcode == IrOpcode::kStoreElement ||
         opcode == IrOpcode::kUnsafePointerAdd || opcode == IrOpcode::kRetain;
}

void StoreStoreElimination::RedundantStoreFinder::Visit(Node* node) {
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

void StoreStoreElimination::RedundantStoreFinder::VisitEffectfulNode(
    Node* node) {
  if (HasBeenVisited(node)) {
    TRACE("- Revisiting: #%d:%s", node->id(), node->op()->mnemonic());
  }
  StoreStoreElimination::UnobservablesSet after_set =
      RecomputeUseIntersection(node);
  StoreStoreElimination::UnobservablesSet before_set =
      RecomputeSet(node, after_set);
  DCHECK(!before_set.IsUnvisited());

  StoreStoreElimination::UnobservablesSet stores_for_node =
      unobservable_for_id(node->id());
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

StoreStoreElimination::UnobservablesSet
StoreStoreElimination::RedundantStoreFinder::RecomputeUseIntersection(
    Node* node) {
  // There were no effect uses. Break early.
  if (node->op()->EffectOutputCount() == 0) {
    IrOpcode::Value opcode = node->opcode();
    // List of opcodes that may end this effect chain. The opcodes are not
    // important to the soundness of this optimization; this serves as a
    // general sanity check. Add opcodes to this list as it suits you.
    //
    // Everything is observable after these opcodes; return the empty set.
    DCHECK_EXTRA(
        opcode == IrOpcode::kReturn || opcode == IrOpcode::kTerminate ||
            opcode == IrOpcode::kDeoptimize || opcode == IrOpcode::kThrow,
        "for #%d:%s", node->id(), node->op()->mnemonic());
    USE(opcode);

    return unobservables_visited_empty_;
  }

  // {first} == true indicates that we haven't looked at any elements yet.
  // {first} == false indicates that cur_set is the intersection of at least one
  // thing.
  bool first = true;
  StoreStoreElimination::UnobservablesSet cur_set =
      StoreStoreElimination::UnobservablesSet::Unvisited();  // irrelevant
  for (Edge edge : node->use_edges()) {
    if (!NodeProperties::IsEffectEdge(edge)) {
      continue;
    }

    // Intersect with the new use node.
    Node* use = edge.from();
    StoreStoreElimination::UnobservablesSet new_set =
        unobservable_for_id(use->id());
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

StoreStoreElimination::UnobservablesSet::UnobservablesSet() : set_(nullptr) {}

StoreStoreElimination::UnobservablesSet
StoreStoreElimination::UnobservablesSet::VisitedEmpty(Zone* zone) {
  ZoneSet<UnobservableStore>* empty_set =
      new (zone->New(sizeof(ZoneSet<UnobservableStore>)))
          ZoneSet<UnobservableStore>(zone);
  return StoreStoreElimination::UnobservablesSet(empty_set);
}

StoreStoreElimination::UnobservablesSet
StoreStoreElimination::UnobservablesSet::Intersect(
    const StoreStoreElimination::UnobservablesSet& other,
    const StoreStoreElimination::UnobservablesSet& empty, Zone* zone) const {
  if (IsEmpty() || other.IsEmpty()) {
    return empty;
  } else {
    ZoneSet<UnobservableStore>* intersection =
        new (zone->New(sizeof(ZoneSet<UnobservableStore>)))
            ZoneSet<UnobservableStore>(zone);
    // Put the intersection of set() and other.set() in intersection.
    set_intersection(set()->begin(), set()->end(), other.set()->begin(),
                     other.set()->end(),
                     std::inserter(*intersection, intersection->end()));

    return StoreStoreElimination::UnobservablesSet(intersection);
  }
}

StoreStoreElimination::UnobservablesSet
StoreStoreElimination::UnobservablesSet::Add(UnobservableStore obs,
                                             Zone* zone) const {
  bool found = set()->find(obs) != set()->end();
  if (found) {
    return *this;
  } else {
    // Make a new empty set.
    ZoneSet<UnobservableStore>* new_set =
        new (zone->New(sizeof(ZoneSet<UnobservableStore>)))
            ZoneSet<UnobservableStore>(zone);
    // Copy the old elements over.
    *new_set = *set();
    // Add the new element.
    bool inserted = new_set->insert(obs).second;
    DCHECK(inserted);
    USE(inserted);  // silence warning about unused variable

    return StoreStoreElimination::UnobservablesSet(new_set);
  }
}

StoreStoreElimination::UnobservablesSet
StoreStoreElimination::UnobservablesSet::RemoveSameOffset(StoreOffset offset,
                                                          Zone* zone) const {
  // Make a new empty set.
  ZoneSet<UnobservableStore>* new_set =
      new (zone->New(sizeof(ZoneSet<UnobservableStore>)))
          ZoneSet<UnobservableStore>(zone);
  // Copy all elements over that have a different offset.
  for (auto obs : *set()) {
    if (obs.offset_ != offset) {
      new_set->insert(obs);
    }
  }

  return StoreStoreElimination::UnobservablesSet(new_set);
}

#undef TRACE
#undef CHECK_EXTRA
#undef DCHECK_EXTRA

}  // namespace compiler
}  // namespace internal
}  // namespace v8
