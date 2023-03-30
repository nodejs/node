// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-interpreter-frame-state.h"

#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph.h"

namespace v8 {
namespace internal {
namespace maglev {

void KnownNodeAspects::Merge(const KnownNodeAspects& other, Zone* zone) {
  DestructivelyIntersect(node_infos, other.node_infos,
                         [](NodeInfo& lhs, const NodeInfo& rhs) {
                           lhs.MergeWith(rhs);
                           return !lhs.is_empty();
                         });
  DestructivelyIntersect(
      stable_maps, other.stable_maps,
      [zone](ZoneHandleSet<Map>& lhs, const ZoneHandleSet<Map>& rhs) {
        for (Handle<Map> map : rhs) {
          lhs.insert(map, zone);
        }
        // We should always add the value even if the set is empty.
        return true;
      });
  DestructivelyIntersect(
      unstable_maps, other.unstable_maps,
      [zone](ZoneHandleSet<Map>& lhs, const ZoneHandleSet<Map>& rhs) {
        for (Handle<Map> map : rhs) {
          lhs.insert(map, zone);
        }
        // We should always add the value even if the set is empty.
        return true;
      });
  DestructivelyIntersect(loaded_constant_properties,
                         other.loaded_constant_properties);
  DestructivelyIntersect(loaded_properties, other.loaded_properties);
  DestructivelyIntersect(loaded_context_constants,
                         other.loaded_context_constants);
  DestructivelyIntersect(loaded_context_slots, other.loaded_context_slots);
}

// static
MergePointInterpreterFrameState* MergePointInterpreterFrameState::New(
    const MaglevCompilationUnit& info, const InterpreterFrameState& state,
    int merge_offset, int predecessor_count, BasicBlock* predecessor,
    const compiler::BytecodeLivenessState* liveness) {
  MergePointInterpreterFrameState* merge_state =
      info.zone()->New<MergePointInterpreterFrameState>(
          info, merge_offset, predecessor_count, 1,
          info.zone()->NewArray<BasicBlock*>(predecessor_count),
          BasicBlockType::kDefault, liveness);
  int i = 0;
  merge_state->frame_state_.ForEachValue(
      info, [&](ValueNode*& entry, interpreter::Register reg) {
        entry = state.get(reg);
        // Initialise the alternatives list and cache the alternative
        // representations of the node.
        Alternatives::List* per_predecessor_alternatives =
            new (&merge_state->per_predecessor_alternatives_[i])
                Alternatives::List();
        per_predecessor_alternatives->Add(info.zone()->New<Alternatives>(
            state.known_node_aspects()->TryGetInfoFor(entry)));
        i++;
      });
  merge_state->predecessors_[0] = predecessor;
  merge_state->known_node_aspects_ =
      state.known_node_aspects()->Clone(info.zone());
  return merge_state;
}

// static
MergePointInterpreterFrameState* MergePointInterpreterFrameState::NewForLoop(
    const InterpreterFrameState& start_state, const MaglevCompilationUnit& info,
    int merge_offset, int predecessor_count,
    const compiler::BytecodeLivenessState* liveness,
    const compiler::LoopInfo* loop_info) {
  MergePointInterpreterFrameState* state =
      info.zone()->New<MergePointInterpreterFrameState>(
          info, merge_offset, predecessor_count, 0,
          info.zone()->NewArray<BasicBlock*>(predecessor_count),
          BasicBlockType::kLoopHeader, liveness);
  if (loop_info->resumable()) {
    state->known_node_aspects_ =
        info.zone()->New<KnownNodeAspects>(info.zone());
    state->is_resumable_loop_ = true;
  }
  auto& assignments = loop_info->assignments();
  auto& frame_state = state->frame_state_;
  int i = 0;
  frame_state.ForEachParameter(
      info, [&](ValueNode*& entry, interpreter::Register reg) {
        entry = nullptr;
        if (assignments.ContainsParameter(reg.ToParameterIndex())) {
          entry = state->NewLoopPhi(info.zone(), reg);
        } else if (state->is_resumable_loop()) {
          // Copy initial values out of the start state.
          entry = start_state.get(reg);
          // Initialise the alternatives list for this value.
          new (&state->per_predecessor_alternatives_[i]) Alternatives::List();
          DCHECK(entry->Is<InitialValue>());
        }
        ++i;
      });
  frame_state.context(info) = nullptr;
  if (state->is_resumable_loop_) {
    // While contexts are always the same at specific locations, resumable loops
    // do have different nodes to set the context across resume points. Create a
    // phi for them.
    frame_state.context(info) = state->NewLoopPhi(
        info.zone(), interpreter::Register::current_context());
  }
  frame_state.ForEachLocal(
      info, [&](ValueNode*& entry, interpreter::Register reg) {
        entry = nullptr;
        if (assignments.ContainsLocal(reg.index())) {
          entry = state->NewLoopPhi(info.zone(), reg);
        }
      });
  DCHECK(!frame_state.liveness()->AccumulatorIsLive());
  return state;
}

// static
MergePointInterpreterFrameState*
MergePointInterpreterFrameState::NewForCatchBlock(
    const MaglevCompilationUnit& unit,
    const compiler::BytecodeLivenessState* liveness, int handler_offset,
    interpreter::Register context_register, Graph* graph, bool is_inline) {
  Zone* const zone = unit.zone();
  MergePointInterpreterFrameState* state =
      zone->New<MergePointInterpreterFrameState>(
          unit, handler_offset, 0, 0, nullptr,
          BasicBlockType::kExceptionHandlerStart, liveness);
  auto& frame_state = state->frame_state_;
  // If the accumulator is live, the ExceptionPhi associated to it is the
  // first one in the block. That ensures it gets kReturnValue0 in the
  // register allocator. See
  // StraightForwardRegisterAllocator::AllocateRegisters.
  if (frame_state.liveness()->AccumulatorIsLive()) {
    frame_state.accumulator(unit) = state->NewExceptionPhi(
        zone, interpreter::Register::virtual_accumulator());
  }
  frame_state.ForEachParameter(
      unit, [&](ValueNode*& entry, interpreter::Register reg) {
        entry = state->NewExceptionPhi(zone, reg);
      });
  frame_state.context(unit) = state->NewExceptionPhi(zone, context_register);
  frame_state.ForEachLocal(unit,
                           [&](ValueNode*& entry, interpreter::Register reg) {
                             entry = state->NewExceptionPhi(zone, reg);
                           });
  state->known_node_aspects_ = zone->New<KnownNodeAspects>(zone);
  return state;
}

MergePointInterpreterFrameState::MergePointInterpreterFrameState(
    const MaglevCompilationUnit& info, int merge_offset, int predecessor_count,
    int predecessors_so_far, BasicBlock** predecessors, BasicBlockType type,
    const compiler::BytecodeLivenessState* liveness)
    : merge_offset_(merge_offset),
      predecessor_count_(predecessor_count),
      predecessors_so_far_(predecessors_so_far),
      predecessors_(predecessors),
      basic_block_type_(type),
      frame_state_(info, liveness),
      per_predecessor_alternatives_(
          info.zone()->NewArray<Alternatives::List>(frame_state_.size(info))) {}

void MergePointInterpreterFrameState::Merge(
    MaglevCompilationUnit& compilation_unit,
    ZoneMap<int, SmiConstant*>& smi_constants, InterpreterFrameState& unmerged,
    BasicBlock* predecessor) {
  DCHECK_GT(predecessor_count_, 1);
  DCHECK_LT(predecessors_so_far_, predecessor_count_);
  predecessors_[predecessors_so_far_] = predecessor;

  if (v8_flags.trace_maglev_graph_building) {
    std::cout << "Merging..." << std::endl;
  }
  int i = 0;
  frame_state_.ForEachValue(compilation_unit, [&](ValueNode*& value,
                                                  interpreter::Register reg) {
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "  " << reg.ToString() << ": "
                << PrintNodeLabel(compilation_unit.graph_labeller(), value)
                << " <- "
                << PrintNodeLabel(compilation_unit.graph_labeller(),
                                  unmerged.get(reg));
    }
    value = MergeValue(compilation_unit, smi_constants, reg,
                       *unmerged.known_node_aspects(), value, unmerged.get(reg),
                       per_predecessor_alternatives_[i]);
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << " => "
                << PrintNodeLabel(compilation_unit.graph_labeller(), value)
                << ": " << PrintNode(compilation_unit.graph_labeller(), value)
                << std::endl;
    }
    ++i;
  });

  if (known_node_aspects_ == nullptr) {
    DCHECK(is_unmerged_loop());
    DCHECK_EQ(predecessors_so_far_, 0);
    known_node_aspects_ = unmerged.known_node_aspects()->CloneForLoopHeader(
        compilation_unit.zone());
  } else {
    known_node_aspects_->Merge(*unmerged.known_node_aspects(),
                               compilation_unit.zone());
  }

  predecessors_so_far_++;
  DCHECK_LE(predecessors_so_far_, predecessor_count_);
}

void MergePointInterpreterFrameState::MergeLoop(
    MaglevCompilationUnit& compilation_unit,
    ZoneMap<int, SmiConstant*>& smi_constants,
    InterpreterFrameState& loop_end_state, BasicBlock* loop_end_block) {
  // This should be the last predecessor we try to merge.
  DCHECK_EQ(predecessors_so_far_, predecessor_count_ - 1);
  DCHECK(is_unmerged_loop());
  predecessors_[predecessor_count_ - 1] = loop_end_block;

  if (v8_flags.trace_maglev_graph_building) {
    std::cout << "Merging loop backedge..." << std::endl;
  }
  frame_state_.ForEachValue(compilation_unit, [&](ValueNode* value,
                                                  interpreter::Register reg) {
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "  " << reg.ToString() << ": "
                << PrintNodeLabel(compilation_unit.graph_labeller(), value)
                << " <- "
                << PrintNodeLabel(compilation_unit.graph_labeller(),
                                  loop_end_state.get(reg));
    }
    MergeLoopValue(compilation_unit, smi_constants, reg,
                   *loop_end_state.known_node_aspects(), value,
                   loop_end_state.get(reg));
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << " => "
                << PrintNodeLabel(compilation_unit.graph_labeller(), value)
                << ": " << PrintNode(compilation_unit.graph_labeller(), value)
                << std::endl;
    }
  });
  predecessors_so_far_++;
  DCHECK_EQ(predecessors_so_far_, predecessor_count_);
}

namespace {

// TODO(victorgomes): Consider refactor this function to share code with
// MaglevGraphBuilder::GetSmiConstant.
SmiConstant* GetSmiConstant(MaglevCompilationUnit& compilation_unit,
                            ZoneMap<int, SmiConstant*>& smi_constants,
                            int constant) {
  DCHECK(Smi::IsValid(constant));
  auto it = smi_constants.find(constant);
  if (it == smi_constants.end()) {
    SmiConstant* node = Node::New<SmiConstant>(compilation_unit.zone(), 0,
                                               Smi::FromInt(constant));
    compilation_unit.RegisterNodeInGraphLabeller(node);
    smi_constants.emplace(constant, node);
    return node;
  }
  return it->second;
}

ValueNode* FromInt32ToTagged(MaglevCompilationUnit& compilation_unit,
                             ZoneMap<int, SmiConstant*>& smi_constants,
                             NodeType node_type, ValueNode* value,
                             BasicBlock* predecessor) {
  DCHECK_EQ(value->properties().value_representation(),
            ValueRepresentation::kInt32);
  DCHECK(!value->properties().is_conversion());

  ValueNode* tagged;
  if (value->Is<Int32Constant>()) {
    int32_t constant = value->Cast<Int32Constant>()->value();
    return GetSmiConstant(compilation_unit, smi_constants, constant);
  } else if (value->Is<StringLength>() ||
             value->Is<BuiltinStringPrototypeCharCodeOrCodePointAt>()) {
    static_assert(String::kMaxLength <= kSmiMaxValue,
                  "String length must fit into a Smi");
    tagged = Node::New<UnsafeSmiTag>(compilation_unit.zone(), {value});
  } else if (NodeTypeIsSmi(node_type)) {
    // For known Smis, we can tag without a check.
    tagged = Node::New<UnsafeSmiTag>(compilation_unit.zone(), {value});
  } else {
    tagged = Node::New<Int32ToNumber>(compilation_unit.zone(), {value});
  }

  predecessor->nodes().Add(tagged);
  compilation_unit.RegisterNodeInGraphLabeller(tagged);
  return tagged;
}

ValueNode* FromUint32ToTagged(MaglevCompilationUnit& compilation_unit,
                              ZoneMap<int, SmiConstant*>& smi_constants,
                              NodeType node_type, ValueNode* value,
                              BasicBlock* predecessor) {
  DCHECK_EQ(value->properties().value_representation(),
            ValueRepresentation::kUint32);
  DCHECK(!value->properties().is_conversion());

  ValueNode* tagged;
  if (NodeTypeIsSmi(node_type)) {
    tagged = Node::New<UnsafeSmiTag>(compilation_unit.zone(), {value});
  } else {
    tagged = Node::New<Uint32ToNumber>(compilation_unit.zone(), {value});
  }

  predecessor->nodes().Add(tagged);
  compilation_unit.RegisterNodeInGraphLabeller(tagged);
  return tagged;
}

ValueNode* FromFloat64ToTagged(MaglevCompilationUnit& compilation_unit,
                               NodeType node_type, ValueNode* value,
                               BasicBlock* predecessor) {
  DCHECK_EQ(value->properties().value_representation(),
            ValueRepresentation::kFloat64);
  DCHECK(!value->properties().is_conversion());

  // Create a tagged version, and insert it at the end of the predecessor.
  ValueNode* tagged =
      Node::New<Float64ToTagged>(compilation_unit.zone(), {value});

  predecessor->nodes().Add(tagged);
  compilation_unit.RegisterNodeInGraphLabeller(tagged);
  return tagged;
}

ValueNode* NonTaggedToTagged(MaglevCompilationUnit& compilation_unit,
                             ZoneMap<int, SmiConstant*>& smi_constants,
                             NodeType node_type, ValueNode* value,
                             BasicBlock* predecessor) {
  switch (value->properties().value_representation()) {
    case ValueRepresentation::kWord64:
    case ValueRepresentation::kTagged:
      UNREACHABLE();
    case ValueRepresentation::kInt32:
      return FromInt32ToTagged(compilation_unit, smi_constants, node_type,
                               value, predecessor);
    case ValueRepresentation::kUint32:
      return FromUint32ToTagged(compilation_unit, smi_constants, node_type,
                                value, predecessor);
    case ValueRepresentation::kFloat64:
      return FromFloat64ToTagged(compilation_unit, node_type, value,
                                 predecessor);
  }
}
ValueNode* EnsureTagged(MaglevCompilationUnit& compilation_unit,
                        ZoneMap<int, SmiConstant*>& smi_constants,
                        const KnownNodeAspects& known_node_aspects,
                        ValueNode* value, BasicBlock* predecessor) {
  if (value->properties().value_representation() ==
      ValueRepresentation::kTagged) {
    return value;
  }

  auto info_it = known_node_aspects.FindInfo(value);
  const NodeInfo* info =
      known_node_aspects.IsValid(info_it) ? &info_it->second : nullptr;
  if (info && info->tagged_alternative) {
    return info->tagged_alternative;
  }
  return NonTaggedToTagged(compilation_unit, smi_constants,
                           info ? info->type : NodeType::kUnknown, value,
                           predecessor);
}

}  // namespace

ValueNode* MergePointInterpreterFrameState::MergeValue(
    MaglevCompilationUnit& compilation_unit,
    ZoneMap<int, SmiConstant*>& smi_constants, interpreter::Register owner,
    const KnownNodeAspects& unmerged_aspects, ValueNode* merged,
    ValueNode* unmerged, Alternatives::List& per_predecessor_alternatives) {
  // If the merged node is null, this is a pre-created loop header merge
  // frame will null values for anything that isn't a loop Phi.
  if (merged == nullptr) {
    DCHECK(is_unmerged_loop());
    DCHECK_EQ(predecessors_so_far_, 0);
    // Initialise the alternatives list and cache the alternative
    // representations of the node.
    new (&per_predecessor_alternatives) Alternatives::List();
    per_predecessor_alternatives.Add(compilation_unit.zone()->New<Alternatives>(
        unmerged_aspects.TryGetInfoFor(unmerged)));
    return unmerged;
  }

  Phi* result = merged->TryCast<Phi>();
  if (result != nullptr && result->merge_state() == this) {
    // It's possible that merged == unmerged at this point since loop-phis are
    // not dropped if they are only assigned to themselves in the loop.
    DCHECK_EQ(result->owner(), owner);
    unmerged = EnsureTagged(compilation_unit, smi_constants, unmerged_aspects,
                            unmerged, predecessors_[predecessors_so_far_]);
    result->set_input(predecessors_so_far_, unmerged);
    return result;
  }

  if (merged == unmerged) {
    // Cache the alternative representations of the unmerged node.
    DCHECK_EQ(per_predecessor_alternatives.LengthForTest(),
              predecessors_so_far_);
    per_predecessor_alternatives.Add(compilation_unit.zone()->New<Alternatives>(
        unmerged_aspects.TryGetInfoFor(unmerged)));
    return merged;
  }

  // Up to this point all predecessors had the same value for this interpreter
  // frame slot. Now that we find a distinct value, insert a copy of the first
  // value for each predecessor seen so far, in addition to the new value.
  // TODO(verwaest): Unclear whether we want this for Maglev: Instead of
  // letting the register allocator remove phis, we could always merge through
  // the frame slot. In that case we only need the inputs for representation
  // selection, and hence could remove duplicate inputs. We'd likely need to
  // attach the interpreter register to the phi in that case?
  result =
      Node::New<Phi>(compilation_unit.zone(), predecessor_count_, this, owner);
  if (v8_flags.trace_maglev_graph_building) {
    for (int i = 0; i < predecessor_count_; i++) {
      result->set_input(i, nullptr);
    }
  }

  if (merged->properties().value_representation() ==
      ValueRepresentation::kTagged) {
    for (int i = 0; i < predecessors_so_far_; i++) {
      result->set_input(i, merged);
    }
  } else {
    // If the existing merged value is untagged, we look through the
    // per-predecessor alternative representations, and try to find tagged
    // representations there.
    int i = 0;
    for (const Alternatives* alt : per_predecessor_alternatives) {
      // TODO(victorgomes): Support Phi nodes of untagged values.
      ValueNode* tagged = alt->tagged_alternative();
      if (tagged == nullptr) {
        tagged = NonTaggedToTagged(compilation_unit, smi_constants,
                                   alt->node_type(), merged, predecessors_[i]);
      }
      result->set_input(i, tagged);
      i++;
    }
    DCHECK_EQ(i, predecessors_so_far_);
  }

  unmerged = EnsureTagged(compilation_unit, smi_constants, unmerged_aspects,
                          unmerged, predecessors_[predecessors_so_far_]);
  result->set_input(predecessors_so_far_, unmerged);

  phis_.Add(result);
  return result;
}
void MergePointInterpreterFrameState::MergeLoopValue(
    MaglevCompilationUnit& compilation_unit,
    ZoneMap<int, SmiConstant*>& smi_constants, interpreter::Register owner,
    KnownNodeAspects& unmerged_aspects, ValueNode* merged,
    ValueNode* unmerged) {
  Phi* result = merged->TryCast<Phi>();
  if (result == nullptr || result->merge_state() != this) {
    // Not a loop phi, we don't have to do anything.
    return;
  }
  DCHECK_EQ(result->owner(), owner);
  unmerged = EnsureTagged(compilation_unit, smi_constants, unmerged_aspects,
                          unmerged, predecessors_[predecessors_so_far_]);
  result->set_input(predecessor_count_ - 1, unmerged);
}

ValueNode* MergePointInterpreterFrameState::NewLoopPhi(
    Zone* zone, interpreter::Register reg) {
  DCHECK_EQ(predecessors_so_far_, 0);
  // Create a new loop phi, which for now is empty.
  Phi* result = Node::New<Phi>(zone, predecessor_count_, this, reg);
  if (v8_flags.trace_maglev_graph_building) {
    for (int i = 0; i < predecessor_count_; i++) {
      result->set_input(i, nullptr);
    }
  }
  phis_.Add(result);
  return result;
}

void MergePointInterpreterFrameState::ReducePhiPredecessorCount(
    interpreter::Register owner, ValueNode* merged) {
  // If the merged node is null, this is a pre-created loop header merge
  // frame with null values for anything that isn't a loop Phi.
  if (merged == nullptr) {
    DCHECK(is_unmerged_loop());
    DCHECK_EQ(predecessors_so_far_, 0);
    return;
  }

  Phi* result = merged->TryCast<Phi>();
  if (result != nullptr && result->merge_state() == this) {
    // It's possible that merged == unmerged at this point since loop-phis are
    // not dropped if they are only assigned to themselves in the loop.
    DCHECK_EQ(result->owner(), owner);
    result->reduce_input_count();
  }
}
}  // namespace maglev
}  // namespace internal
}  // namespace v8
