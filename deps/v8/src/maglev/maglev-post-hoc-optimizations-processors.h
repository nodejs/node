// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_POST_HOC_OPTIMIZATIONS_PROCESSORS_H_
#define V8_MAGLEV_MAGLEV_POST_HOC_OPTIMIZATIONS_PROCESSORS_H_

#include <type_traits>

#include "src/compiler/heap-refs.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-cse.h"
#include "src/maglev/maglev-deopt-frame-visitor.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-known-node-aspects.h"
#include "src/numbers/conversions.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::maglev {

// Recomputes the use hints for all Phi nodes in the graph. This is
// necessary after inlining, as the original use hints may be out of date.
// For example, a Phi node in the caller's graph might now be used by a node
// from the inlined function, and this new use needs to be recorded.
//
// This processor first clears all existing use hints on all Phi nodes. Then,
// it iterates through all nodes in the graph and re-calculates the use hints
// for any Phi nodes that are used as inputs.
class RecomputePhiUseHintsProcessor {
 public:
#define TRACE_PHI_USE_HINTS(x)                                \
  do {                                                        \
    if (V8_UNLIKELY(v8_flags.trace_maglev_phi_untagging)) {   \
      StdoutStream() << "[phi use hints] " << x << std::endl; \
    }                                                         \
  } while (false)

  explicit RecomputePhiUseHintsProcessor(Zone* zone) : live_loop_phis_(zone) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {
#ifdef DEBUG
    // The recording below trusts the graph's top-frame sets to be exhaustive:
    // a top frame referencing a Phi that isn't registered would be skipped,
    // silently letting the Phi be truncated. Enforce the invariant that any
    // unregistered top frame is Phi-free (e.g. the FunctionEntryStackCheck
    // frame, which is never registered but has no Phis at function entry).
    // TODO(victorgomes): Move this check to the Maglev verifier and make frame
    // registration a global invariant (kept live across escape analysis and the
    // classic-Maglev LICM frame rewrites), so it can run at every pipeline
    // point instead of only here.
    auto references_phi = [](const auto* deopt_info) {
      bool found = false;
      deopt_info->ForEachInput([&](ValueNode* input) {
        if (input->Is<Phi>()) found = true;
      });
      return found;
    };
    for (BasicBlock* block : graph->blocks()) {
      if (block->is_dead()) continue;
      block->ForEachNodeAndControl([&](NodeBase* node) {
        if (node->properties().has_eager_deopt_info()) {
          const EagerDeoptInfo* info = node->eager_deopt_info();
          DCHECK_IMPLIES(
              !graph->eager_deopt_top_frames().contains(&info->top_frame()),
              !references_phi(info));
        }
        if (node->properties().can_lazy_deopt()) {
          const LazyDeoptInfo* info = node->lazy_deopt_info();
          DCHECK_IMPLIES(
              !graph->lazy_deopt_top_frames().contains(&info->top_frame()),
              !references_phi(info));
        }
      });
    }
#endif  // DEBUG
    // Record kNonTruncated for Phis referenced by deopt frames, walking the
    // graph's deduplicated top-frame sets instead of every node's frame. This
    // uses the non-const ForEachInput, which also unwraps the frames and moves
    // their deopt uses onto the unwrapped nodes, which UnwrapDeoptFrames relies
    // on. Relies on every live top frame being registered there (the loop
    // peeler registers its cloned frames).
    auto record_deopt_use = [](ValueNode* input) {
      Phi* phi = input->TryCast<Phi>();
      if (!phi) return;
      phi->RecordUseReprHint(
          UseRepresentationSet{UseRepresentation::kNonTruncated},
          /*force_same_loop=*/false);
      TRACE_PHI_USE_HINTS("updating deopt-frame use hints for "
                          << PrintNodeLabel(phi)
                          << ": use_reprs=" << phi->use_repr_hints());
    };
    for (DeoptFrame* top_frame : graph->eager_deopt_top_frames()) {
      EagerDeoptInfo info(graph->zone(), top_frame, {});
      info.ForEachInput(record_deopt_use);
    }
    for (auto [top_frame, result_location] : graph->lazy_deopt_top_frames()) {
      LazyDeoptInfo info(graph->zone(), top_frame, result_location.first,
                         result_location.second, {});
      info.ForEachInput(record_deopt_use);
    }
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    if (!block->has_phi()) return BlockProcessResult::kContinue;
    Phi::List& phis = *block->phis();
    bool is_loop_header = block->is_loop();
    for (auto it = phis.begin(); it != phis.end(); ++it) {
      TRACE_PHI_USE_HINTS(
          "cleaning use hints for "
          << PrintNodeLabel(*it)
          << " previous values:  use_reprs=" << (*it)->use_repr_hints()
          << " and same_loop_use_reprs=" << (*it)->same_loop_use_repr_hints());
      it->ClearUseHints();
      if (is_loop_header) {
        live_loop_phis_.insert(*it);
      }
    }
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

  ProcessResult Process(JumpLoop* node, const ProcessingState& state) {
    BasicBlock* loop_header = node->target();
    DCHECK(loop_header->is_loop());
    if (!loop_header->has_phi()) return ProcessResult::kContinue;
    Phi::List& phis = *loop_header->phis();
    for (auto it = phis.begin(); it != phis.end(); ++it) {
      for (Input input : it->inputs()) {
        if (!input.node()) continue;
        if (Phi* input_phi = input.node()->TryCast<Phi>()) {
          input_phi->RecordUseReprHint((*it)->use_repr_hints());
          TRACE_PHI_USE_HINTS("updating use hints for "
                              << PrintNodeLabel(input_phi)
                              << ": use_reprs=" << input_phi->use_repr_hints()
                              << " and same_loop_use_reprs="
                              << input_phi->same_loop_use_repr_hints());
        }
      }
      DCHECK(live_loop_phis_.contains(*it));
      live_loop_phis_.erase(*it);
    }
    return ProcessResult::kContinue;
  }

  ProcessResult Process(Phi* node, const ProcessingState& state) {
    return ProcessResult::kContinue;
  }

  ProcessResult Process(CheckSmi* node, const ProcessingState& state) {
    return ProcessResult::kContinue;
  }

  ProcessResult Process(AssumeType* node, const ProcessingState& state) {
    return ProcessResult::kContinue;
  }

  ProcessResult Process(NodeBase* node, const ProcessingState& state) {
    DCHECK(!node->Is<Phi>());
    if (ValueNode* value_node = node->TryCast<ValueNode>()) {
      if (value_node->use_count() == 0 &&
          !value_node->properties().is_required_when_unused()) {
        return ProcessResult::kContinue;
      }
    }
    for (Input input : node->inputs()) {
      if (!input.node()) continue;
      if (Phi* phi = input.node()->TryCast<Phi>()) {
        UseRepresentation use_repr = UseRepresentation::kTagged;
        if (node->is_conversion()) {
          use_repr = UseRepresentationFromValue(
              node->Cast<ValueNode>()->value_representation());
        } else if (node->Is<ReturnedValue>()) {
          ValueNode* unwrapped = node->input_node(0);
          while (unwrapped->Is<ReturnedValue>()) {
            unwrapped = unwrapped->input_node(0);
          }
          DCHECK(!unwrapped->is_conversion());
          use_repr =
              UseRepresentationFromValue(unwrapped->value_representation());
        } else if (IsTruncatingToInt32(node->opcode())) {
          use_repr = UseRepresentation::kTruncatedInt32;
        } else if (node->Is<NumberToString>()) {
          use_repr = UseRepresentation::kTaggedForNumberToString;
        } else if (node->Is<CheckedNumberOrOddballToUint8Clamped>()) {
          use_repr = UseRepresentation::kHoleyFloat64;
        }
        phi->RecordUseReprHint(UseRepresentationSet{use_repr},
                               live_loop_phis_.contains(phi));
        TRACE_PHI_USE_HINTS(
            "updating use hints for "
            << PrintNodeLabel(phi) << ": use_reprs=" << phi->use_repr_hints()
            << " and same_loop_use_reprs=" << phi->same_loop_use_repr_hints()
            << " after visiting input " << PrintNode(node));
      }
    }
    // Deopt-frame uses are recorded in PostProcessGraph.
    return ProcessResult::kContinue;
  }

 private:
  ZoneAbslFlatHashSet<Phi*> live_loop_phis_;

  constexpr UseRepresentation UseRepresentationFromValue(
      ValueRepresentation repr) {
    switch (repr) {
      case ValueRepresentation::kInt32:
        return UseRepresentation::kInt32;
      case ValueRepresentation::kUint32:
        return UseRepresentation::kUint32;
      case ValueRepresentation::kFloat64:
        return UseRepresentation::kFloat64;
      case ValueRepresentation::kHoleyFloat64:
        return UseRepresentation::kHoleyFloat64;
      default:
        return UseRepresentation::kTagged;
    }
    UNREACHABLE();
  }
#undef TRACE_PHI_USE_HINTS
};

// Optimizations involving loops which cannot be done at graph building time.
// Currently mainly loop invariant code motion.
class LoopOptimizationProcessor {
 public:
  explicit LoopOptimizationProcessor(MaglevCompilationInfo* info)
      : zone(info->zone()) {
    was_deoptimized =
        info->toplevel_compilation_unit()->feedback().was_once_deoptimized();
  }

  void PreProcessGraph(Graph* graph) {}
  void PostPhiProcessing() {}

  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    current_block = block;
    if (current_block->is_loop()) {
      loop_effects = current_block->state()->loop_effects();
      if (loop_effects) return BlockProcessResult::kContinue;
    } else {
      // TODO(olivf): Some dominance analysis would allow us to keep loop
      // effects longer than just the first block of the loop.
      loop_effects = nullptr;
    }
    return BlockProcessResult::kSkip;
  }

  bool IsLoopPhi(Node* input) {
    DCHECK(current_block->is_loop());
    if (auto phi = input->TryCast<Phi>()) {
      if (phi->is_loop_phi() && phi->merge_state() == current_block->state()) {
        return true;
      }
    }
    return false;
  }

  bool CanHoist(Node* candidate) {
    DCHECK_EQ(candidate->input_count(), 1);
    DCHECK(current_block->is_loop());
    ValueNode* input = candidate->input(0).node();
    DCHECK(!IsLoopPhi(input));
    // For hoisting an instruction we need:
    // * A unique loop entry block.
    // * Inputs live before the loop (i.e., not defined inside the loop).
    // * No hoisting over checks (done eagerly by clearing loop_effects).
    // TODO(olivf): We should enforce loops having a unique entry block at graph
    // building time.
    if (current_block->predecessor_count() != 2) return false;
    BasicBlock* loop_entry = current_block->predecessor_at(0);
    if (loop_entry->successors().size() != 1) {
      return false;
    }
    if (IsConstantNode(input->opcode())) return true;
    return input->owner() != current_block;
  }

  ProcessResult Process(LoadContextSlotNoCells* ltf,
                        const ProcessingState& state) {
    DCHECK(loop_effects);
    ValueNode* object = ltf->ValueInput().node();
    if (IsLoopPhi(object)) {
      return ProcessResult::kContinue;
    }
    auto key = std::tuple{object, ltf->offset()};
    if (!loop_effects->may_have_aliasing_contexts &&
        !loop_effects->unstable_aspects_cleared &&
        !loop_effects->context_slot_written.count(key) && CanHoist(ltf)) {
      return ProcessResult::kHoist;
    }
    return ProcessResult::kContinue;
  }

  ProcessResult Process(LoadTaggedField* ltf, const ProcessingState& state) {
    if (ltf->property_key().type() != PropertyKey::kName) {
      return ProcessResult::kContinue;
    }
    return ProcessNamedLoad(ltf, ltf->ValueInput().node(), ltf->property_key());
  }

  ProcessResult Process(StringLength* len, const ProcessingState& state) {
    return ProcessNamedLoad(len, len->StringInput().node(),
                            PropertyKey::StringLength());
  }

  ProcessResult Process(LoadTypedArrayLength* len,
                        const ProcessingState& state) {
    return ProcessNamedLoad(len, len->ValueInput().node(),
                            PropertyKey::TypedArrayLength());
  }

  ProcessResult ProcessNamedLoad(Node* load, ValueNode* object,
                                 PropertyKey name) {
    DCHECK(!load->properties().can_deopt());
    if (!loop_effects) return ProcessResult::kContinue;
    if (IsLoopPhi(object)) {
      return ProcessResult::kContinue;
    }
    if (!loop_effects->unstable_aspects_cleared &&
        !loop_effects->keys_cleared.count(name) &&
        !loop_effects->objects_written.count(object) && CanHoist(load)) {
      return ProcessResult::kHoist;
    }
    return ProcessResult::kContinue;
  }

  ProcessResult Process(CheckMaps* maps, const ProcessingState& state) {
    DCHECK(loop_effects);
    // Hoisting a check out of a loop can cause it to trigger more than actually
    // needed (i.e., if the loop is executed 0 times). This could lead to
    // deoptimization loops as there is no feedback to learn here. Thus, we
    // abort this optimization if the function deoptimized previously. Also, if
    // hoisting of this check fails we need to abort (and not continue) to
    // ensure we are not hoisting other instructions over it.
    if (was_deoptimized) return ProcessResult::kSkipBlock;
    ValueNode* object = maps->ReceiverInput().node();
    if (IsLoopPhi(object)) {
      return ProcessResult::kSkipBlock;
    }
    if (!loop_effects->unstable_aspects_cleared &&
        !loop_effects->elements_kind_transitioned && CanHoist(maps)) {
      if (auto j = current_block->predecessor_at(0)
                       ->control_node()
                       ->TryCast<CheckpointedJump>()) {
        maps->SetEagerDeoptInfo(
            zone, zone->New<DeoptFrame>(j->eager_deopt_info()->top_frame()),
            maps->eager_deopt_info()->feedback_to_update());
        return ProcessResult::kHoist;
      }
    }
    return ProcessResult::kSkipBlock;
  }

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    // Ensure we are not hoisting over checks.
    if (node->properties().can_eager_deopt()) {
      loop_effects = nullptr;
      return ProcessResult::kSkipBlock;
    }
    return ProcessResult::kContinue;
  }

  void PostProcessGraph(Graph* graph) {}

  Zone* zone;
  BasicBlock* current_block;
  const LoopEffects* loop_effects;
  bool was_deoptimized;
};

template <typename NodeT>
constexpr bool CanBeStoreToNonEscapedObject() {
  return CanBeStoreToNonEscapedObject(NodeBase::opcode_of<NodeT>);
}

class AnyUseMarkingProcessor {
 public:
  // TODO(victorgomes): extract the escape analysis to a separate processor.
  explicit AnyUseMarkingProcessor(bool run_maglev_escape_analysis = true)
      : run_maglev_escape_analysis_(run_maglev_escape_analysis) {}

  void PreProcessGraph(Graph* graph) {}
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    if constexpr (IsValueNode(Node::opcode_of<NodeT>) &&
                  (!NodeT::kProperties.is_required_when_unused() ||
                   std::is_same_v<ArgumentsElements, NodeT>)) {
      if (!node->is_used()) {
        if (!node->unused_inputs_were_visited()) {
          DropInputUses(node);
        }
        if constexpr (std::is_same_v<NodeT, InitialValue>) {
          node->mark_unused();
        }
        return ProcessResult::kRemove;
      }
    }

    if constexpr (CanBeStoreToNonEscapedObject<NodeT>()) {
      if (node->input(0).node()->template Is<InlinedAllocation>()) {
        stores_to_allocations_.push_back(node);
      }
    }

    return ProcessResult::kContinue;
  }

  ProcessResult Process(Dead* node, const ProcessingState& state) {
    return ProcessResult::kRemove;
  }

  void PostProcessGraph(Graph* graph) {
    if (run_maglev_escape_analysis_) {
      RunEscapeAnalysis(graph);
      DropUseOfValueInStoresToCapturedAllocations();
      DCHECK(drop_uses_stack_.empty());
    }
  }

 private:
  std::vector<Node*> stores_to_allocations_;
  base::SmallVector<ValueNode*, 8> drop_uses_stack_;

  void EscapeAllocation(Graph* graph, InlinedAllocation* alloc,
                        Graph::SmallAllocationVector& deps) {
    if (alloc->HasBeenAnalysed() && alloc->HasEscaped()) return;
    alloc->SetEscaped();
    for (auto dep : deps) {
      EscapeAllocation(graph, dep,
                       graph->allocations_escape_map().find(dep)->second);
    }
  }

  void VerifyEscapeAnalysis(Graph* graph) {
#ifdef DEBUG
    for (const auto& it : graph->allocations_escape_map()) {
      auto* alloc = it.first;
      DCHECK(alloc->HasBeenAnalysed());
      if (alloc->HasEscaped()) {
        for (auto* dep : it.second) {
          DCHECK(dep->HasEscaped());
        }
      }
    }
#endif  // DEBUG
  }

  void RunEscapeAnalysis(Graph* graph) {
    for (auto& it : graph->allocations_escape_map()) {
      auto* alloc = it.first;
      if (alloc->HasBeenAnalysed()) continue;
      // Check if all its uses are non escaping.
      if (alloc->HasEscapingUses()) {
        // Escape this allocation and all its dependencies.
        EscapeAllocation(graph, alloc, it.second);
      } else {
        // Try to capture the allocation. This can still change if a escaped
        // allocation has this value as one of its dependencies.
        alloc->SetElided();
      }
    }
    // Check that we've reached a fixpoint.
    VerifyEscapeAnalysis(graph);
  }

  void DropUseOfValueInStoresToCapturedAllocations() {
    for (Node* node : stores_to_allocations_) {
      InlinedAllocation* alloc =
          node->input(0).node()->Cast<InlinedAllocation>();
      // Since we don't analyze if allocations will escape until a fixpoint,
      // this could drop an use of an allocation and turn it non-escaping.
      if (alloc->HasBeenAnalysed() && alloc->HasBeenElided()) {
        // Skip first input.
        for (int i = 1; i < node->input_count(); i++) {
          DropInputUses(node->input(i));
        }
      }
    }
  }

  void RemoveUseAndPushIfUnused(Input input) {
    ValueNode* input_node = input.node();
    if (input_node->properties().is_required_when_unused() &&
        !input_node->Is<ArgumentsElements>()) {
      return;
    }
    input_node->remove_use();
    if (!input_node->is_used() && !input_node->unused_inputs_were_visited()) {
      input_node->mark_unused_inputs_visited();
      drop_uses_stack_.push_back(input_node);
    }
  }

  void DrainDropUsesStack() {
    while (!drop_uses_stack_.empty()) {
      ValueNode* current = drop_uses_stack_.back();
      drop_uses_stack_.pop_back();
      DCHECK(!current->properties().can_eager_deopt());
      DCHECK(!current->properties().can_lazy_deopt());
      for (Input input : current->inputs()) {
        RemoveUseAndPushIfUnused(input);
      }
    }
  }

  void DropInputUses(Input input) {
    DCHECK(drop_uses_stack_.empty());
    RemoveUseAndPushIfUnused(input);
    DrainDropUsesStack();
  }

  void DropInputUses(ValueNode* node) {
    DCHECK(drop_uses_stack_.empty());
    DCHECK(!node->unused_inputs_were_visited());
    drop_uses_stack_.push_back(node);
    node->mark_unused_inputs_visited();
    DrainDropUsesStack();
  }

  bool run_maglev_escape_analysis_;
};

class DeadNodeSweepingProcessor {
 public:
  void PreProcessGraph(Graph* graph) {
    if (graph->has_graph_labeller()) {
      labeller_ = graph->graph_labeller();
    }
  }
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

  ProcessResult Process(AllocationBlock* node, const ProcessingState& state) {
    // Note: this need to be done before ValueLocationConstraintProcessor, since
    // it access the allocation offsets.
    int size = 0;
    for (auto alloc : node->allocation_list()) {
      if (!alloc->HasBeenAnalysed() || alloc->HasEscaped()) {
        alloc->set_offset(size);
        size += alloc->size();
      }
    }
    // ... and update its size.
    node->set_size(size);
    // If size is zero, then none of the inlined allocations have escaped, we
    // can remove the allocation block.
    if (size == 0) return ProcessResult::kRemove;
    return ProcessResult::kContinue;
  }

  ProcessResult Process(InlinedAllocation* node, const ProcessingState& state) {
    // Remove inlined allocation that became non-escaping.
    if (node->HasBeenAnalysed() && node->HasBeenElided()) {
      if (v8_flags.trace_maglev_escape_analysis) {
        std::cout << "* Removing allocation node " << PrintNodeLabel(node)
                  << std::endl;
      }
      return ProcessResult::kRemove;
    }
    return ProcessResult::kContinue;
  }

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    if constexpr (CanBeStoreToNonEscapedObject<NodeT>()) {
      if (V8_UNLIKELY(v8_flags.trace_maglev_escape_analysis) &&
          IsSweepableDeadNode(node)) {
        InlinedAllocation* object =
            node->input(0).node()->template Cast<InlinedAllocation>();
        std::cout << "* Removing store node " << PrintNodeLabel(node)
                  << " to allocation " << PrintNodeLabel(object) << std::endl;
      }
    }

    if (IsSweepableDeadNode(node)) return ProcessResult::kRemove;

    return ProcessResult::kContinue;
  }

  template <typename NodeT>
  static bool IsSweepableDeadNode(NodeT* node) {
    if (IsDead(node)) return true;

    if constexpr (CanBeStoreToNonEscapedObject<NodeT>()) {
      if (InlinedAllocation* object =
              node->input(0).node()->template TryCast<InlinedAllocation>()) {
        if (!object->HasBeenAnalysed()) return false;
        if (!object->HasEscaped()) return true;
      }
    }

    return false;
  }

 private:
  MaglevGraphLabeller* labeller_ = nullptr;
};

// Tracks which exception handlers are reachable by collecting catch blocks
// from throwing nodes. Unreachable exception handlers (and their successors)
// are aborted and marked dead.
class ReachableExceptionHandlerTracker {
 public:
  explicit ReachableExceptionHandlerTracker(Graph* graph)
      : graph_(graph), reachable_exception_handlers_(graph->zone()) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

  void MarkReachable(BasicBlock* block) {
    reachable_exception_handlers_.insert(block);
  }

  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    // TODO(victorgomes): Support removing the unreachable blocks instead of
    // just skipping it.
    if (V8_UNLIKELY(block->IsUnreachable())) {
      return AbortBlock(block);
    }

    if (block->is_exception_handler_block()) {
      if (!IsReachable(block)) {
        return AbortBlock(block);
      }
    }
    return BlockProcessResult::kContinue;
  }

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    if constexpr (NodeT::kProperties.can_throw()) {
      if (node->exception_handler_info()->HasExceptionHandler() &&
          !node->exception_handler_info()->ShouldLazyDeopt()) {
        MarkReachable(node->exception_handler_info()->catch_block());
      }
    }
    return ProcessResult::kContinue;
  }

 private:
  BlockProcessResult AbortBlock(BasicBlock* block) {
    ControlNode* control = block->reset_control_node();
    block->RemovePredecessorFollowing(control);
    control->OverwriteWith<Abort>()->set_reason(AbortReason::kUnreachable);
    block->set_deferred(true);
    block->set_control_node(control);
    block->mark_dead();
    graph_->set_may_have_unreachable_blocks();
    return BlockProcessResult::kSkip;
  }

  bool IsReachable(BasicBlock* block) const {
    return reachable_exception_handlers_.contains(block);
  }

  Graph* graph_;
  ZoneAbslFlatHashSet<BasicBlock*> reachable_exception_handlers_;
};

template <typename KnaProvider>
class CommonSubexpressionEliminationProcessor {
 public:
  explicit CommonSubexpressionEliminationProcessor(KnaProvider& kna_provider)
      : kna_provider_(kna_provider) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  void PostPhiProcessing() {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    if constexpr (ShouldCSE(Node::opcode_of<NodeT>) &&
                  IsFixedInputNode<NodeT>()) {
      // A value node is CSE'd by overwriting it in place with an Identity to
      // its equivalent. The Identity stores the equivalent in input slot 0, so
      // a zero-input value cannot be eliminated here. Checks produce no value
      // and are just removed, so they stay eligible regardless of input count.
      // TODO(victorgomes): Support CSE of zero-input value nodes.
      if constexpr (!std::is_base_of_v<ValueNode, NodeT> ||
                    NodeT::kInputCount >= 1) {
        return TryCSE(node);
      }
    }
    return ProcessResult::kContinue;
  }

 private:
  static constexpr bool ShouldCSE(Opcode op) {
    return Node::participate_in_cse(op) && !Node::needs_epoch_check(op);
  }

  KnownNodeAspects& known_node_aspects() {
    return kna_provider_.known_node_aspects();
  }

  template <typename NodeT>
  ProcessResult TryCSE(NodeT* node) {
    static_assert(IsFixedInputNode<NodeT>());
    static_assert(Node::participate_in_cse(Node::opcode_of<NodeT>));
    static_assert(!Node::needs_epoch_check(Node::opcode_of<NodeT>));
    std::array<ValueNode*, NodeT::kInputCount> inputs;
    int i = 0;
    for (ValueNode*& input : inputs) {
      input = node->input_node(i++)->UnwrapIdentities();
    }
    cse::CanonicalizeCommutative<NodeT>(inputs);
    uint32_t hash = cse::HashNode(node, inputs);
    NodeT* equivalent = std::apply(
        [&](const auto&... opts) {
          return known_node_aspects().template FindExpression<NodeT>(
              hash, inputs, opts...);
        },
        node->options());
    if (equivalent) {
      // Value nodes forward their uses to the equivalent via an Identity;
      // checks have no result, so a plain removal suffices.
      if constexpr (std::is_base_of_v<ValueNode, NodeT>) {
        node->OverwriteWithIdentityTo(equivalent);
      }
      return ProcessResult::kRemove;
    }
    known_node_aspects().AddExpression(hash, node);
    return ProcessResult::kContinue;
  }

  KnaProvider& kna_provider_;
};

// TODO(victorgomes): This eliminator is block-local -- it clears its state at
// every basic-block boundary (PreProcessBasicBlock), so a bounds check only
// subsumes redundant checks, and only certifies Smi-safe indices, within the
// same block. Extend it to a dominator-tree / whole-graph walk so a check in a
// dominating block can eliminate checks (and fold Smi-size artifacts) in
// dominated successor blocks.
class BoundsCheckEliminationProcessor {
 public:
  explicit BoundsCheckEliminationProcessor(Graph* graph)
      : graph_(graph),
        current_block_bounds_checks_(graph->zone()),
        current_block_smi_safe_(graph->zone()) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  void PostPhiProcessing() {}
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }

  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    current_block_ = block;
    current_block_bounds_checks_.clear();
    current_block_smi_safe_.clear();
    scanned_current_block_ = false;
    return BlockProcessResult::kContinue;
  }

  ProcessResult Process(CheckTypedArrayBounds* node,
                        const ProcessingState& state) {
    if (TryElide(node, state)) {
      return ProcessResult::kRemove;
    }
    return ProcessResult::kContinue;
  }

  ProcessResult Process(CheckInt32Condition* node,
                        const ProcessingState& state) {
    RecordIfSmiSafeIndex(node);
    if (TryElide(node, state)) {
      return ProcessResult::kRemove;
    }
    return ProcessResult::kContinue;
  }

  ProcessResult Process(CheckInt32IsSmi* node, const ProcessingState&) {
    if (IsKnownSmiSafe(node->input_node(0))) {
      return ProcessResult::kRemove;
    }
    return ProcessResult::kContinue;
  }

  ProcessResult Process(CheckedSmiSizedInt32* node, const ProcessingState&) {
    if (IsKnownSmiSafe(node->input_node(0))) {
      node->OverwriteWithIdentityTo(node->input_node(0));
      return ProcessResult::kRemove;
    }
    return ProcessResult::kContinue;
  }

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    return ProcessResult::kContinue;
  }

 private:
  struct BoundsCheckInfo {
    int32_t max_index;
    bool emitted = false;
  };

  bool TryElide(Node* node, const ProcessingState& state) {
    int32_t index = 0;
    ValueNode* length = nullptr;
    if (!TryGetConstantBoundsCheck(node, &index, &length)) {
      return false;
    }

    if (!scanned_current_block_) {
      FindMaxConstantIndicesInBlock(state.node_index(), index, length);
    }

    auto it = current_block_bounds_checks_.find(length);
    if (it == current_block_bounds_checks_.end()) {
      return false;
    }

    auto& [max_index, emitted] = it->second;
    if (!emitted) {
      // Rewrite the very first bounds check in the block to check the maximum
      // index.
      if (index < max_index) {
        node->change_input(0, graph_->GetInt32Constant(max_index));
      }
      emitted = true;
      return false;
    } else if (index > max_index) {
      // This bounds check did not exist when FindMaxConstantIndicesInBlock was
      // called. Its index is larger than the maximum we found then, so we have
      // to emit a bounds check.

      // Future bounds checks can be elided if their index is less than the
      // index of this bounds check.
      max_index = index;
      return false;
    }
    // Any subsequent constant bounds checks on this length are redundant.
    return true;
  }

  bool TryGetConstantBoundsCheck(Node* node, int32_t* index_val,
                                 ValueNode** length) {
    ValueNode* index = nullptr;
    if (auto* typed_bounds_check = node->TryCast<CheckTypedArrayBounds>()) {
      index = typed_bounds_check->IndexInput().node();
      *length = typed_bounds_check->LengthInput().node();
    } else if (auto* int_bounds_check = node->TryCast<CheckInt32Condition>()) {
      if (int_bounds_check->condition() == AssertCondition::kUnsignedLessThan) {
        index = int_bounds_check->input_node(0);
        *length = int_bounds_check->input_node(1);
      } else {
        return false;
      }
    } else {
      return false;
    }
    if (std::optional<int32_t> const_index = TryGetInt32Constant(index)) {
      if (*const_index >= 0) {
        *index_val = *const_index;
        return true;
      }
    }
    return false;
  }

  // TODO(ahaas): This is a copy of MaglevReducer::TryGetInt32Constant. We
  // should share this logic.
  std::optional<int32_t> TryGetInt32Constant(ValueNode* value) {
    switch (value->opcode()) {
      case Opcode::kHeapConstant: {
        compiler::ObjectRef object = value->Cast<HeapConstant>()->object();
        if (object.IsHeapNumber() &&
            IsInt32Double(object.AsHeapNumber().value())) {
          return static_cast<int32_t>(object.AsHeapNumber().value());
        }
        return {};
      }
      case Opcode::kInt32Constant:
        return value->Cast<Int32Constant>()->value();
      case Opcode::kUint32Constant: {
        uint32_t uint32_value = value->Cast<Uint32Constant>()->value();
        if (uint32_value <= INT32_MAX) {
          return static_cast<int32_t>(uint32_value);
        }
        return {};
      }
      case Opcode::kSmiConstant:
        return value->Cast<SmiConstant>()->value().value();
      case Opcode::kFloat64Constant: {
        double double_value =
            value->Cast<Float64Constant>()->value().get_scalar();
        if (!IsInt32Double(double_value)) return {};
        return FastD2I(double_value);
      }
      default:
        break;
    }
    return {};
  }

  void FindMaxConstantIndicesInBlock(int start_index, int32_t initial_index_val,
                                     ValueNode* initial_length) {
    scanned_current_block_ = true;
    // This function gets called when the first bounds check in the block with a
    // constant index is encountered. We can insert it directly into the map. We
    // then have to scan the rest of the block for other bounds checks with
    // constant indices.
    current_block_bounds_checks_.insert(
        {initial_length,
         BoundsCheckInfo{initial_index_val, /*emitted=*/false}});
    const auto& nodes = current_block_->nodes();
    for (size_t i = start_index + 1; i < nodes.size(); ++i) {
      Node* node = nodes[i];
      if (node == nullptr) continue;
      int32_t index = 0;
      ValueNode* length = nullptr;
      if (TryGetConstantBoundsCheck(node, &index, &length)) {
        auto it = current_block_bounds_checks_.find(length);
        if (it != current_block_bounds_checks_.end()) {
          it->second.max_index = std::max(it->second.max_index, index);
        } else {
          current_block_bounds_checks_.insert(
              {length, BoundsCheckInfo{index, /*emitted=*/false}});
        }
      }
    }
  }

  void RecordIfSmiSafeIndex(CheckInt32Condition* node) {
    if (node->condition() != AssertCondition::kUnsignedLessThan) return;
    if (!IsSmiBoundedLength(node->input_node(1))) return;
    current_block_smi_safe_.insert(node->input_node(0)->UnwrapIdentities());
  }

  bool IsKnownSmiSafe(ValueNode* node) {
    return current_block_smi_safe_.contains(node->UnwrapIdentities());
  }

  bool IsSmiBoundedLength(ValueNode* length) {
    length = length->UnwrapIdentities();
    while (length->Is<CheckedSmiUntag>() || length->Is<UnsafeSmiUntag>()) {
      length = length->input_node(0)->UnwrapIdentities();
    }
    if (auto* load = length->TryCast<LoadTaggedField>()) {
      return load->is_array_length() == IsArrayLength::kYes &&
             load->type() == NodeType::kSmi;
    }
    if (std::optional<int32_t> c = TryGetInt32Constant(length)) {
      return *c >= 0 && *c <= Smi::kMaxValue;
    }
    return false;
  }

  Graph* graph_;
  BasicBlock* current_block_ = nullptr;
  ZoneMap<ValueNode*, BoundsCheckInfo> current_block_bounds_checks_;
  ZoneAbslFlatHashSet<ValueNode*> current_block_smi_safe_;
  bool scanned_current_block_ = false;
};

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_MAGLEV_POST_HOC_OPTIMIZATIONS_PROCESSORS_H_
