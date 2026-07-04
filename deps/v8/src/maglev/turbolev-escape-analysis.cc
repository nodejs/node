// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/turbolev-escape-analysis.h"

#include <optional>

#include "absl/container/flat_hash_set.h"
#include "src/base/container-utils.h"
#include "src/compiler/js-heap-broker.h"
#include "src/diagnostics/code-tracer.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/fixed-primitive-array.h"

namespace v8::internal::maglev {

#define TRACE TRACE_ESCAPE_ANALYSIS
#define NODE_ID(n) labeller()->NodeId(n)
#define BLOCK_ID(b) b->id()
#define PRINT_NODE(n) "n" << NODE_ID(n) << ": " << PrintNode(n)

bool IsUnusedAndCanBeRemoved(ValueNode* node) {
  if (node->properties().is_required_when_unused()) return false;
  if (node->Is<ArgumentsElements>()) return false;
  return !node->is_used();
}

ValueNode* EscapeAnalysisData::Get(InlinedAllocation* base, int offset) {
  DCHECK_EQ(base, TryGetCandidateInlinedAllocation(base));
  ObjectField addr = ObjectField{base, offset};
  Key key = TryGetKeyFor(addr);
  DCHECK(key.valid());
  return field_values.Get(key);
}

Key EscapeAnalysisData::GetOrCreateKey(InlinedAllocation* base, int offset) {
  ObjectField addr = ObjectField{base, offset};
  auto it = keys_mappings.find(addr);
  if (it != keys_mappings.end()) {
    return it->second;
  } else {
    Key key = field_values.NewKey(addr);
    keys_mappings.insert({addr, key});
    return key;
  }
}

base::SmallVector<compiler::MapRef, 8> EscapeAnalysisData::GetMaps(
    InlinedAllocation* alloc) {
  DCHECK_EQ(alloc, TryGetCandidateInlinedAllocation(alloc));
  ValueNode* map_node = Get(alloc, offsetof(HeapObject, map_));
  DCHECK_NOT_NULL(map_node);

  ZoneAbslFlatHashSet<ValueNode*> seen(zone);
  base::SmallVector<compiler::MapRef, 8> maps;
  base::SmallVector<ValueNode*, 8> worklist;

  auto maybe_push = [&seen, &worklist](ValueNode* node) {
    // Phis could be self-referencing / mutually recursive, so we need to avoid
    // infinite loops that could be caused by visiting multiple times the same
    // node.
    if (seen.contains(node)) return;
    seen.insert(node);
    worklist.push_back(node);
  };

  maybe_push(map_node);
  while (!worklist.empty()) {
    ValueNode* curr = worklist.back();
    worklist.pop_back();

    compiler::OptionalHeapObjectRef maybe_constant =
        curr->TryGetConstant(broker);
    if (maybe_constant.has_value()) {
      DCHECK(maybe_constant->IsMap());
      maps.push_back(maybe_constant->AsMap());
    } else {
      DCHECK(curr->Is<Phi>());
      for (Input input : curr->inputs()) {
        maybe_push(input.node());
      }
    }
  }

  // TODO(dmercadier): It could be worth to remove duplicates from {maps} (which
  // can happen for instance in a 3-way merge when 2 of the 3 predecessors have
  // the same value for a map).
  return maps;
}

ValueNode* EscapeAnalysisData::ResolveBase(ValueNode* node,
                                           int predecessor_index) {
  switch (node->opcode()) {
    case Opcode::kLoadTaggedField: {
      LoadTaggedField* load = node->Cast<LoadTaggedField>();
      return ResolveLoadBase(load->ValueInput().node(), load->offset(), load,
                             predecessor_index);
    }

    case Opcode::kIdentity:
      return ResolveBase(node->input(0), predecessor_index);

    default:
      return node;
  }
}

ValueNode* EscapeAnalysisData::ResolveLoadBase(ValueNode* base, int offset,
                                               ValueNode* fallback,
                                               int predecessor_index) {
  if (InlinedAllocation* alloc =
          TryGetCandidateInlinedAllocation(base, predecessor_index)) {
    ObjectField addr = ObjectField{alloc, offset};
    Key key = TryGetKeyFor(addr);
    if (!key.valid()) {
      // {alloc} is a valid candidate, but somehow it doesn't have a value for
      // field {offset}. This can happen 1) in unreachable code (because for
      // instance one branch underwent a transition and another doesn't, in
      // which case a map checks should prevent the current node from being
      // reached, but it wasn't folded away for some reason), and 2) possibly in
      // loops (where this would be unreachable in the 1st iteration, but a
      // transition later down the loop body would make this accessible in
      // subsequent iterations). For now, we just mark {alloc} as escaping when
      // this happens.
      // TODO(dmercadier): Avoid marking {alloc} as escaping somehow.
      MarkAsEscaped(alloc);
      return fallback;
    }
    DCHECK(key.valid());
    if (predecessor_index == -1) {
      return field_values.Get(key);
    } else {
      return field_values.GetPredecessorValue(key, predecessor_index);
    }
  }
  return fallback;
}

Key EscapeAnalysisData::TryGetKeyFor(ObjectField addr) {
  auto key_it = keys_mappings.find(addr);
  if (key_it != keys_mappings.end()) {
    return key_it->second;
  }
  return Key{};
}

InlinedAllocation* EscapeAnalysisData::TryGetCandidateInlinedAllocation(
    ValueNode* node, int predecessor_index) {
  node = ResolveBase(node, predecessor_index);
  if (InlinedAllocation* alloc = node->TryCast<InlinedAllocation>()) {
    if (candidates.contains(alloc) &&
        candidates.at(alloc) != CandidateStatus::kCannotElide) {
      return alloc;
    }
  }
  return nullptr;
}

void EscapeAnalysisData::RecordDependentAllocation(InlinedAllocation* base,
                                                   InlinedAllocation* value) {
  if (!(candidates.contains(base) &&
        candidates.at(base) != CandidateStatus::kCannotElide)) {
    MarkAsEscaped(value);
    return;
  }

  DCHECK(candidates.contains(base));
  if (!alloc_dependencies.contains(base)) {
    alloc_dependencies.insert(
        {base, zone->template New<ZoneUnorderedSet<InlinedAllocation*>>(zone)});
  }
  alloc_dependencies.at(base)->insert(value);
}

void EscapeAnalysisData::MarkAsEscapedIfCandidate(ValueNode* node,
                                                  int predecessor_index) {
  if (InlinedAllocation* alloc =
          TryGetCandidateInlinedAllocation(node, predecessor_index)) {
    MarkAsEscaped(alloc);
  }
}

void EscapeAnalysisData::MarkAsEscaped(InlinedAllocation* alloc) {
  TRACE("Marking candidate:" << NODE_ID(alloc) << " as escaping");
  if (!(alloc->HasBeenAnalysed() && alloc->HasEscaped())) alloc->SetEscaped();
  candidates.at(alloc) = CandidateStatus::kCannotElide;
  if (alloc_dependencies.contains(alloc)) {
    for (InlinedAllocation* other : *alloc_dependencies.at(alloc)) {
      if (candidates.contains(other) &&
          candidates.at(other) != CandidateStatus::kCannotElide) {
        MarkAsEscaped(other);
      }
    }
  }
}

namespace {

void UpdateVirtualObjects(DeoptFrame* deopt_frame,
                          VirtualObjectList& virtual_objects) {
  if (deopt_frame->type() == DeoptFrame::FrameType::kInterpretedFrame) {
    deopt_frame->as_interpreted().set_last_virtual_object(
        virtual_objects.head());
    return;
  }
  DCHECK_NOT_NULL(deopt_frame->parent());
  return UpdateVirtualObjects(deopt_frame->parent(), virtual_objects);
}

void AddUsesToInputs(
    VirtualObject* vobj,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map) {
  // Because VirtualObjects could be recursive (or mutually recursive), we use
  // {seen} to prevent infinite loop while visiting inputs of VirtualObjects.

  // TODO(dmercadier): Use Zone structures for {seen} and {worklist} and
  // allocate them in a way that the memory can be reused every time we call
  // AddUsesToInputs (eg, make this function a member of EscapeAnalysisData and
  // make {seen} and {worklist} members there as well).
  absl::flat_hash_set<VirtualObject*> seen;
  std::vector<VirtualObject*> worklist;
  worklist.push_back(vobj);

  while (!worklist.empty()) {
    VirtualObject* curr = worklist.back();
    worklist.pop_back();

    if (seen.contains(curr)) continue;
    seen.insert(curr);

    for (int i = 0; i < curr->slot_count(); i++) {
      ValueNode* input = curr->get_by_index(i);
      input->add_use();
      if (InlinedAllocation* alloc = input->TryCast<InlinedAllocation>()) {
        auto it = vobj_map.find(alloc);
        if (it != vobj_map.end()) {
          worklist.push_back(it->second);
        }
      }
    }
  }
}

ValueNode* GetUpdatedValue(
    ValueNode* value,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map,
    EscapeAnalysisData& data) {
  ValueNode* resolved = data.ResolveBase(value);
  if (resolved != value) {
    // TODO(dmercadier): we can't reduce the use-count of {value} here, because
    // when we duplicate DeoptFrames we don't increase use-counts to account
    // for uses in the cloned DeoptFrame. This is sound because we will also
    // never remove the uses corresponding to the original DeoptFrame that was
    // dropped (which means that the use-count of {value} or any input of the
    // current DeoptFrame cannot go down to 0). However, this whole thing sounds
    // a bit messy and it would be better to track more accurate use counts.
    resolved->add_use();
  }

  // If {resolved} is a VirtualObject, we need to add uses to its inputs.
  auto it = vobj_map.find(resolved);
  if (it != vobj_map.end()) {
    AddUsesToInputs(it->second, vobj_map);
  }

  return resolved;
}

void UpdateInterpretedFrame(
    InterpretedDeoptFrame& frame,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map,
    EscapeAnalysisData& data) {
  frame.closure() = GetUpdatedValue(frame.closure(), vobj_map, data);

  frame.frame_state()->ForEachValue(
      frame.unit(),
      [&vobj_map, &data](ValueNode*& value, interpreter::Register owner) {
        value = GetUpdatedValue(value, vobj_map, data);
      });
}

void UpdateInlinedArgumentsFrame(
    InlinedArgumentsDeoptFrame& frame,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map,
    EscapeAnalysisData& data) {
  frame.closure() = GetUpdatedValue(frame.closure(), vobj_map, data);

  for (ValueNode*& value : frame.arguments()) {
    value = GetUpdatedValue(value, vobj_map, data);
  }
}

void UpdateConstructInvokeStubFrame(
    ConstructInvokeStubDeoptFrame& frame,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map,
    EscapeAnalysisData& data) {
  // Note that while the receiver cannot be elided, it could still be a
  // LoadTaggedField from an elided base, in which case it still need to be
  // updated to bypass the load.
  frame.receiver() = GetUpdatedValue(frame.receiver(), vobj_map, data);
  frame.context() = GetUpdatedValue(frame.context(), vobj_map, data);
}

void UpdateBuiltinContinuationFrame(
    BuiltinContinuationDeoptFrame& frame,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map,
    EscapeAnalysisData& data) {
  frame.context() = GetUpdatedValue(frame.context(), vobj_map, data);

  for (ValueNode*& value : frame.parameters()) {
    value = GetUpdatedValue(value, vobj_map, data);
  }
}

// TODO(dmercadier): UpdateFrameState could probably reuse
// DeoptInfoVisitor::VisitSingleFrame.
void UpdateFrameState(
    DeoptFrame* frame_state,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map,
    EscapeAnalysisData& data) {
  if (frame_state->parent() != nullptr) {
    UpdateFrameState(frame_state->parent(), vobj_map, data);
  }

  switch (frame_state->type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      UpdateInterpretedFrame(frame_state->as_interpreted(), vobj_map, data);
      return;
    case DeoptFrame::FrameType::kInlinedArgumentsFrame:
      UpdateInlinedArgumentsFrame(frame_state->as_inlined_arguments(), vobj_map,
                                  data);
      return;
    case DeoptFrame::FrameType::kConstructInvokeStubFrame:
      UpdateConstructInvokeStubFrame(frame_state->as_construct_stub(), vobj_map,
                                     data);
      return;
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      UpdateBuiltinContinuationFrame(frame_state->as_builtin_continuation(),
                                     vobj_map, data);
      return;
  }
}

CompactInterpreterFrameState* CloneCompactInterpreterFrameState(
    const CompactInterpreterFrameState* input_frame_state,
    const MaglevCompilationUnit& info, Zone* zone) {
  CompactInterpreterFrameState* new_frame_state =
      zone->New<CompactInterpreterFrameState>(info,
                                              input_frame_state->liveness());

  new_frame_state->ForEachValue(
      info, [&](ValueNode*& entry, interpreter::Register reg) {
        entry = input_frame_state->GetValueOf(reg, info);
      });

  return new_frame_state;
}

InterpretedDeoptFrame* CloneInterpretedFrame(
    const InterpretedDeoptFrame& input_frame, DeoptFrame* parent, Zone* zone) {
  CompactInterpreterFrameState* frame_state = CloneCompactInterpreterFrameState(
      input_frame.frame_state(), input_frame.unit(), zone);

  return zone->New<InterpretedDeoptFrame>(
      input_frame.unit(), frame_state, input_frame.closure(),
      input_frame.last_virtual_object(), input_frame.bytecode_position(),
      input_frame.source_position(), parent);
}

InlinedArgumentsDeoptFrame* CloneInlinedArgumentsFrame(
    const InlinedArgumentsDeoptFrame& input_frame, DeoptFrame* parent,
    Zone* zone) {
  return zone->New<InlinedArgumentsDeoptFrame>(
      input_frame.unit(), input_frame.bytecode_position(),
      input_frame.closure(), zone->CloneVector(input_frame.arguments()),
      parent);
}

ConstructInvokeStubDeoptFrame* CloneConstructInvokeStubFrame(
    const ConstructInvokeStubDeoptFrame& input_frame, DeoptFrame* parent,
    Zone* zone) {
  return zone->New<ConstructInvokeStubDeoptFrame>(
      input_frame.unit(), input_frame.source_position(), input_frame.receiver(),
      input_frame.context(), parent);
}

BuiltinContinuationDeoptFrame* CloneBuiltinContinuationFrame(
    const BuiltinContinuationDeoptFrame& input_frame, DeoptFrame* parent,
    Zone* zone) {
  return zone->New<BuiltinContinuationDeoptFrame>(
      input_frame.builtin_id(), zone->CloneVector(input_frame.parameters()),
      input_frame.context(),
      input_frame.is_javascript() ? input_frame.javascript_target()
                                  : compiler::OptionalJSFunctionRef(),
      parent);
}

// TODO(dmercadier): share the frame cloning helpers with MaglevLoopPeeler
// (which has CloneDeoptFrame).
DeoptFrame* DeepClone(const DeoptFrame& input_frame, Zone* zone) {
  // TODO(dmercadier): we don't really need to clone all DeoptFrames: we just
  // need to clone the ones that have elided InlinedAllocation inputs.
  DeoptFrame* parent = nullptr;
  if (input_frame.parent() != nullptr) {
    parent = DeepClone(*input_frame.parent(), zone);
  }

  switch (input_frame.type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      return CloneInterpretedFrame(input_frame.as_interpreted(), parent, zone);
    case DeoptFrame::FrameType::kInlinedArgumentsFrame:
      return CloneInlinedArgumentsFrame(input_frame.as_inlined_arguments(),
                                        parent, zone);
    case DeoptFrame::FrameType::kConstructInvokeStubFrame:
      return CloneConstructInvokeStubFrame(input_frame.as_construct_stub(),
                                           parent, zone);
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      return CloneBuiltinContinuationFrame(
          input_frame.as_builtin_continuation(), parent, zone);
  }
}

}  // namespace

void EscapeAnalysis::Run(Graph* graph, MaglevCompilationInfo* compilation_info,
                         Zone* phase_zone) {
  EscapeAnalysis analyzer(graph, compilation_info, phase_zone);
  analyzer.AnalyzeCandidates();

  if (!analyzer.HasElidableCandidates()) {
    Tracer tracer_(compilation_info);
    TRACE("No potential candidate found, stopping.");
    return;
  }

  analyzer.ElideCandidates();
}

class CandidateAnalyzer {
 public:
  explicit CandidateAnalyzer(EscapeAnalysisData& data)
      : data_(data), tracer_(data.compilation_info) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

  ProcessResult Process(InlinedAllocation* node, const ProcessingState&) {
    if (node->HasBeenAnalysed() && node->HasEscaped()) {
      TRACE("Not recording already-escaping candidate");
      return ProcessResult::kContinue;
    }

    TRACE("Recording potential candidate " << PRINT_NODE(node));

    // The GraphBuilder never creates ConsStrings for Turbolev since Turboshaft
    // has a StringEscapeAnalysis phase that takes care of elided non-escaping
    // NewConsString and StringConcat.
    // TODO(dmercadier): handle ConsStrings here as well; now that there is this
    // analysis there is no point in keeping StringEscapeAnalysis in Turboshaft.
    DCHECK_NE(node->object()->object_type(), vobj::ObjectType::kConsString);

    // If we're revisiting a loop, then {node} could already have an entry in
    // {candidates}, in which case its status shouldn't be kCannoElide
    // (otherwise the initial `if` in this function should have early-returned).
    DCHECK_IMPLIES(candidates().contains(node),
                   candidates().at(node) != CandidateStatus::kCannotElide);

    // Recording as a new candidate for eliding. Note that, as mentioned above,
    // their might already be an entry for {node} in {candidates}, but we can
    // just safely override it.
    candidates().insert({node, CandidateStatus::kCanMaybeElide});

    return ProcessResult::kContinue;
  }

  ProcessResult Process(AssertEscapeAnalysisElided* node,
                        const ProcessingState&) {
    TRACE(
        "Process AssertEscapeAnalysisElided in CandidateAnalyzer: doing "
        "nothing");
    return ProcessResult::kContinue;
  }

  ProcessResult Process(StoreMap* node, const ProcessingState&) {
    if (InlinedAllocation* alloc =
            data_.TryGetCandidateInlinedAllocation(node->ValueInput().node())) {
      TRACE("Process(StoreMap) for candidate "
            << NODE_ID(alloc) << " (StoreMap id=" << NODE_ID(node) << ")");
      ValueNode* map_node = data_.graph->GetConstant(node->map());
      ProcessFieldStore(node, alloc, offsetof(HeapObject, map_), map_node);
      return ProcessResult::kContinue;
    }
    return ProcessResult::kContinue;
  }

  void ProcessFieldStore(NodeBase* node, ValueNode* base, int offset,
                         ValueNode* value) {
    TRACE("ProcessFieldStore: base=" << NODE_ID(base) << " offset=" << offset
                                     << " value=" << NODE_ID(value));
    value = data_.ResolveBase(value);
    if (InlinedAllocation* alloc =
            data_.TryGetCandidateInlinedAllocation(base)) {
      TRACE(" > base is actually candidate:" << NODE_ID(base)
                                             << " => recording");
      Key key = data_.GetOrCreateKey(alloc, offset);

      if (InlinedAllocation* value_alloc =
              data_.TryGetCandidateInlinedAllocation(value)) {
        TRACE(" > Value is candidate:" << NODE_ID(value_alloc)
                                       << " => recording dependency");
        data_.RecordDependentAllocation(alloc, value_alloc);
      }

      DCHECK_NOT_NULL(key.data().base);
      DCHECK_NOT_NULL(value);
      field_values().Set(key, value);
    } else {
      TRACE(
          " > base is not candidate; checking if value needs to be marked as "
          "escaping.");
      data_.MarkAsEscapedIfCandidate(value);
    }
  }

  ProcessResult Process(StoreTaggedFieldNoWriteBarrier* node,
                        const ProcessingState&) {
    TRACE("Process " << PRINT_NODE(node));
    ProcessFieldStore(node, node->ObjectInput().node(), node->offset(),
                      node->ValueInput().node());
    return ProcessResult::kContinue;
  }

  ProcessResult Process(StoreTaggedFieldWithWriteBarrier* node,
                        const ProcessingState&) {
    TRACE("Process " << PRINT_NODE(node));
    ProcessFieldStore(node, node->ObjectInput().node(), node->offset(),
                      node->ValueInput().node());
    return ProcessResult::kContinue;
  }

  ProcessResult Process(StoreInt32* node, const ProcessingState&) {
    TRACE("Process " << PRINT_NODE(node));
    ProcessFieldStore(node, node->ObjectInput().node(), node->offset(),
                      node->ValueInput().node());
    return ProcessResult::kContinue;
  }

  template <typename FixedArrayT, typename StoreT>
  ProcessResult ProcessFixedArrayStore(StoreT* node) {
    TRACE("Process " << PRINT_NODE(node));
    if (Int32Constant* index_node =
            node->IndexInput().node()->template TryCast<Int32Constant>()) {
      int offset = FixedArrayT::OffsetOfElementAt(index_node->value());
      ProcessFieldStore(node, node->ElementsInput().node(), offset,
                        node->ValueInput().node());
    } else {
      // TODO(dmercadier): handle non-constant indices. This will require
      // stack-allocating the array.
      data_.MarkAsEscapedIfCandidate(node->ElementsInput().node());
      data_.MarkAsEscapedIfCandidate(node->ValueInput().node());
    }

    return ProcessResult::kContinue;
  }

  ProcessResult Process(StoreFixedArrayElementNoWriteBarrier* node,
                        const ProcessingState&) {
    return ProcessFixedArrayStore<FixedArray>(node);
  }

  ProcessResult Process(StoreFixedArrayElementWithWriteBarrier* node,
                        const ProcessingState&) {
    return ProcessFixedArrayStore<FixedArray>(node);
  }

  ProcessResult Process(StoreFixedDoubleArrayElement* node,
                        const ProcessingState&) {
    return ProcessFixedArrayStore<FixedDoubleArray>(node);
  }

  ProcessResult Process(StoreFloat64* node, const ProcessingState&) {
    TRACE("Process " << PRINT_NODE(node));
    ProcessFieldStore(node, node->ObjectInput().node(), node->offset(),
                      node->ValueInput().node());
    return ProcessResult::kContinue;
  }

  bool IsGuaranteedToPassMapCheck(InlinedAllocation* alloc,
                                  const compiler::ZoneRefSet<Map>& maps) {
    DCHECK_EQ(alloc, data_.TryGetCandidateInlinedAllocation(alloc));
    // We need to make sure that all of the maps that this object could have
    // at this point would pass this CheckMaps. If it isn't the case, then we
    // just bail out on eliding this candidate.
    // TODO(dmercadier): it's possible that control-flow is set up in a way
    // that this CheckMaps will indeed pass but we don't realize it. For
    // instance:
    //
    //    let o = { x : 42 }; // MapA
    //    if (b) { o.y = 17; } // transition to MapB
    //    if (b) { return o.y; }
    //
    // Here, `return o.y` will lead to inserting a CheckMaps, but at this point
    // the set of possible maps with contain both MapA and MapB (assuming no
    // double-diamond elimination of course), and so we'll wrongly mark this
    // candidate as escaping. Instead of marking {alloc} as escaped, we could
    // just keep going, and in the Elider, replace this CheckMaps by a similar
    // node that doesn't load the map but take it as input.
    auto possible_maps = data_.GetMaps(alloc);

    for (compiler::MapRef object_map : possible_maps) {
      bool map_found = false;
      for (compiler::MapRef allowed_map : maps) {
        if (object_map == allowed_map) {
          map_found = true;
          break;
        }
      }
      if (!map_found) {
        // //TODO(dmercadier): Here we could just proceed and then insert a
        // CheckMapsWithAlreadyLoadedMap with the dynamic Map as input. Except
        // that CheckMapsWithAlreadyLoadedMap takes the Object as input, but I
        // don't think that it needs to.
        return false;
      }
    }

    return true;
  }

  template <typename CheckMapsT>
  ProcessResult ProcessCheckMaps(CheckMapsT* node) {
    TRACE("Process " << PRINT_NODE(node));
    if (InlinedAllocation* alloc = data_.TryGetCandidateInlinedAllocation(
            node->ReceiverInput().node())) {
      TRACE(" > receiver is candidate:" << NODE_ID(alloc));

      if (!IsGuaranteedToPassMapCheck(alloc, node->maps())) {
        // TODO(dmercadier): we could also make sure that at least one of
        // {possible_maps} is in {allowed_map}; otherwise we can just deopt (but
        // it might still be worth to elide the object, since this might be a
        // path that's never reached at runtime!). Well not quite: this is also
        // called from CheckMapsWithMigration, which shouldn't deopt when the
        // map is wrong but instead try to migrate it...
        TRACE(
            " >> object doesn't have the correct map. Removing from "
            "candidates");
        data_.MarkAsEscaped(alloc);
      } else {
        TRACE(" >> object is guranteed to pass Map check; will remove");
      }
      return ProcessResult::kContinue;
    }
    return ProcessResult::kContinue;
  }

  ProcessResult Process(CheckMaps* node, const ProcessingState&) {
    return ProcessCheckMaps(node);
  }
  ProcessResult Process(CheckMapsWithMigration* node, const ProcessingState&) {
    return ProcessCheckMaps(node);
  }
  ProcessResult Process(CheckMapsWithMigrationAndDeopt* node,
                        const ProcessingState&) {
    return ProcessCheckMaps(node);
  }

  ProcessResult Process(AssumeMap*, const ProcessingState&) {
    // Shouldn't make its input escape.
    return ProcessResult::kContinue;
  }

  ProcessResult Process(LoadTaggedField* node, const ProcessingState&) {
    // Shouldn't make its input escape.
    return ProcessResult::kContinue;
  }

  ProcessResult Process(LoadFixedArrayElement* node,
                        const ProcessingState& state) {
    // LoadFixedArrayElement should never be used for Int32Constant index, and
    // thus should never be elided (for now).
    DCHECK(!node->IndexInput().node()->Is<Int32Constant>());
    // TODO(dmercadier): handle non-constant indices. This will require
    // stack-allocating the array. For now, we just go to the generic Process
    // overload.
    return Process(static_cast<NodeBase*>(node), state);
  }

  ProcessResult Process(LoadFloat64* node, const ProcessingState&) {
    TRACE("Process " << PRINT_NODE(node));
    // Not escaping input.
    return ProcessResult::kContinue;
  }

  ProcessResult Process(LoadFixedDoubleArrayElement* node,
                        const ProcessingState&) {
    // Only need to escape the input if the input is non-constant.
    if (!node->IndexInput().node()->Is<Int32Constant>()) {
      // TODO(dmercadier): handle non-constant indices. This will require
      // stack-allocating the array.
      data_.MarkAsEscapedIfCandidate(node->ElementsInput().node());
    }
    return ProcessResult::kContinue;
  }

  ProcessResult Process(BranchIfReferenceEqual* node, const ProcessingState&) {
    // No need to invalidate: if either {left_input} or {right_input} can be
    // elided but not the other one, then they are not the same object. And if
    // they both can be elided, then we can check if they are the same object or
    // not.
    return ProcessResult::kContinue;
  }

  void ProcessPhi(Phi* phi) {
    // TODO(dmercadier): enable merging escape analysis candidates when they
    // flow into Phis.
    for (int i = 0; i < phi->input_count(); i++) {
      ValueNode* input = phi->input_node(i);
      // Note the `, i` in the call to MarkAsEscapedIfCandidate: this is the
      // reason for this overload: not all Phi inputs dominate the current block
      // (they only have to dominate the corresponding predecessor), and so this
      // `i` will be used when querying the FieldValuesTable to look up values
      // in `i`th predecessor instead of in the current block.
      data_.MarkAsEscapedIfCandidate(input, i);
    }
  }

  ProcessResult Process(Phi* phi, const ProcessingState&) {
    ProcessPhi(phi);
    return ProcessResult::kContinue;
  }

  template <class NodeT>
    requires std::is_base_of_v<NodeBase, NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState&) {
    if constexpr (std::is_base_of_v<ValueNode, NodeT>) {
      if (IsUnusedAndCanBeRemoved(static_cast<ValueNode*>(node))) {
        // TODO(dmercadier): consider running proper DCE before escape analysis
        // to not block escape analysis on dead nodes.
        return ProcessResult::kContinue;
      }
    }

    for (Input input : node->inputs()) {
      ValueNode* input_node = data_.ResolveBase(input);
      data_.MarkAsEscapedIfCandidate(input_node);
    }

    if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      // Because lazy frame states can generate stack trace, we must prevent
      // eliding their "receiver" and "function" fields.
      MarkFrameStateReceiverAndFunctionAsEscaping(
          node->lazy_deopt_info()->top_frame());
    }

    return ProcessResult::kContinue;
  }

  // TODO(dmercadier): could this use DeoptInfoVisitor::VisitSingleFrame?
  void MarkFrameStateReceiverAndFunctionAsEscaping(const DeoptFrame& frame) {
    if (frame.parent() != nullptr) {
      MarkFrameStateReceiverAndFunctionAsEscaping(*frame.parent());
    }

    switch (frame.type()) {
      case DeoptFrame::FrameType::kInterpretedFrame: {
        const InterpretedDeoptFrame& interpreted_frame = frame.as_interpreted();
        data_.MarkAsEscapedIfCandidate(interpreted_frame.closure());
        interpreted_frame.frame_state()->ForEachValue(
            interpreted_frame.unit(),
            [&](ValueNode* node, interpreter::Register reg) {
              // TODO(dmercadier): maglev checks function_closure here, but I
              // don't think that's possible right? Or, if it is, it should be a
              // duplicate with interprete_frame.closure, no?
              DCHECK_NE(reg, interpreter::Register::function_closure());
              if (reg == interpreter::Register::receiver() ||
                  reg == interpreter::Register::function_closure()) {
                data_.MarkAsEscapedIfCandidate(node);
              }
            });
        break;
      }

      case DeoptFrame::FrameType::kBuiltinContinuationFrame: {
        const BuiltinContinuationDeoptFrame& continuation_frame =
            frame.as_builtin_continuation();
        if (!continuation_frame.parameters().empty()) {
          data_.MarkAsEscapedIfCandidate(
              continuation_frame.parameters().first());
        }
        break;
      }

      case DeoptFrame::FrameType::kInlinedArgumentsFrame:
      case DeoptFrame::FrameType::kConstructInvokeStubFrame:
        break;
    }
  }

 private:
  Zone* zone() { return data_.zone; }
  FieldValuesTable& field_values() { return data_.field_values; }
  ZoneAbslFlatHashMap<ObjectField, Key>& keys_mappings() {
    return data_.keys_mappings;
  }
  ZoneAbslFlatHashMap<BasicBlock*, ZoneVector<std::pair<Phi*, Key>>*>&
  new_phis() {
    return data_.new_phis;
  }
  MaglevGraphLabeller* labeller() { return data_.labeller(); }
  Candidates& candidates() { return data_.candidates; }

  EscapeAnalysisData& data_;
  Tracer tracer_;
};

class FieldValuesTracker : public CandidateAnalyzer {
 public:
  explicit FieldValuesTracker(EscapeAnalysisData& data)
      : CandidateAnalyzer(data),
        data_(data),
        block_snapshots_(data.zone),
        tracer_(data.compilation_info) {}

  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    TRACE("PreProcessBasicBlock " << BLOCK_ID(block));

    auto new_phis_it = new_phis().find(block);
    if (new_phis_it != new_phis().end()) {
      // We are revisiting this block because of a loop.
      TRACE("> clearing previous new Phis");
      new_phis_it->second->clear();
    }

    if (block->is_exception_handler_block()) {
      // TODO(dmercadier): we would need to compute the state from the
      // predecessors, but predecessors are not available. For now, we're just
      // nuking everything. While iterating the graph, we could record exception
      // handlers predecessor in a side table fairly easily.
      base::SmallVector<InlinedAllocation*, 8> to_mark_as_escaped;
      for (auto candidate : data_.candidates) {
        to_mark_as_escaped.push_back(candidate.first);
      }
      for (InlinedAllocation* alloc : to_mark_as_escaped) {
        data_.MarkAsEscaped(alloc);
      }
    }

    CreateSnapshotFor(block);

    return BlockProcessResult::kContinue;
  }

  bool CreateSnapshotFor(BasicBlock* block) {
    TRACE("> CreateSnapshotFor " << BLOCK_ID(block));
    DCHECK(field_values().IsSealed());
    base::SmallVector<Snapshot, 4> predecessors_snapshots;
    block->ForEachPredecessor([&](BasicBlock* pred) {
      MaybeSnapshot pred_snapshot = block_snapshots_[pred];
      if (pred_snapshot.has_value()) {
        predecessors_snapshots.push_back(pred_snapshot.value());
      } else {
        // If we are visiting a loop header for the 1st time, then the backedge
        // predecessor won't have a value yet.
        DCHECK(block->is_loop() &&
               pred == block->predecessor_at(block->predecessor_count() - 1));
      }
    });

    int predecessor_count = static_cast<int>(predecessors_snapshots.size());

    std::optional<MergePointInterpreterFrameState*> state;
    DCHECK_IMPLIES(predecessor_count > 1, block->has_state());
    if (block->has_state()) {
      state = block->state();
    }

    bool created_phi = false;
    auto merge_field_values =
        [&](Key key,
            base::Vector<ValueNode* const> predecessors) -> ValueNode* {
      DCHECK_NOT_NULL(key.data().base);
      bool all_predecessors_equal = true;
      for (ValueNode* pred : predecessors) {
        if (pred == nullptr) {
          // This means that the allocation is not available on all predecessor
          // paths. This is not an issue: either it will flow into a phi, in
          // which case it will be invalidated, or it doesn't, in which case
          // it's not "available" anymore and thus doesn't escape some subgraph
          // (like a loop body for instance).
          return nullptr;
        }
        if (pred != predecessors[0]) {
          all_predecessors_equal = false;
        }
      }
      if (all_predecessors_equal) {
        // All of the predecessors have the same value recorded for {key}, so
        // there is no need to insert a Phi.
        return predecessors[0];
      }

#ifdef DEBUG
      // All predecessors should have the same value_representation.
      ValueRepresentation first_pred_repr =
          predecessors[0]->value_representation();
      for (int i = 1; i < predecessor_count; i++) {
        DCHECK_EQ(first_pred_repr, predecessors[i]->value_representation());
      }
#endif

      DCHECK(state.has_value());
      // The "owner" field of Phis is just used for exception phis late in the
      // Maglev backend (register allocator / code generator), which new Phis
      // inserted here will never be used for (since this phase is only used for
      // Turbolev). As such, we can use anything for the owner, and thus use
      // `Register::invalid_value`.
      constexpr interpreter::Register kFakeOwner =
          interpreter::Register::invalid_value();
      // TODO(dmercadier): instead of creating a proper Phi (which are 64 bytes
      // long + inputs!), we could have a custom "PseudoPhi" (name tbd)
      // structure that contains the bare minimum and would basically just be a
      // vector of Union(ValueNodes, PseudoPhi)., and only create real Phis in
      // the elider once we're sure that we're going to need them.
      Phi* phi = NodeBase::New<Phi>(zone(), predecessor_count, state.value(),
                                    kFakeOwner);
#ifdef V8_ENABLE_MAGLEV_GRAPH_PRINTER
      // TODO(dmercadier): should we register Phis only once we're sure that
      // we'll add them to the graph? (which happens in
      // EliderBase::PostProcessBasicBlock)
      labeller()->RegisterNode(phi);
#endif
      for (int i = 0; i < predecessor_count; i++) {
        phi->set_input(i, predecessors[i]);
      }
      phi->change_representation(predecessors[0]->value_representation());
      TRACE(">> Created new phi: " << PRINT_NODE(phi));
      created_phi = true;
      RegisterNewPhi(phi, block, key);

      // We need to call `ProcessPhi` because an input of this phi could be an
      // eliding candidate, in which case we need to invalidate it.
      CandidateAnalyzer::ProcessPhi(phi);

      return phi;
    };

    field_values().StartNewSnapshot(base::VectorOf(predecessors_snapshots),
                                    merge_field_values);

    return created_phi;
  }

  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    DCHECK(!field_values().IsSealed());
    bool already_had_snapshot = block_snapshots_[block].has_value();

    Snapshot snapshot = field_values().Seal();
    block_snapshots_[block] = MaybeSnapshot{snapshot};

    if (JumpLoop* jump_loop = block->control_node()->TryCast<JumpLoop>()) {
      BasicBlock* loop_header = jump_loop->target();

      if (already_had_snapshot) {
        // We only consider revisiting the loop once, ie, if the backedge didn't
        // already have a snapshot.

        // Note however that the loop header might already have loop phis from
        // the previous visit, in which case we might need to update them.
        PatchLoopPhisBackedges(loop_header, snapshot);

        return BlockProcessResult::kContinue;
      }

      // TODO(dmercadier): we could try to reuse the snapshot created by
      // CreateSnapshotFor when revisiting the loop, instead of discarding it
      // and recomputing it afterwards.
      if (CheckLoopPhiInvalidation(loop_header) ||
          CreateSnapshotFor(loop_header)) {
        TRACE("> Will revisit loop");
        // Discarding temporary snapshot.
        if (!field_values().IsSealed()) field_values().Seal();
        // TODO(dmercadier): instead of revisiting loops one by one, we could
        // instead re-run the whole processor (similar to what the
        // `RangeProcessor` does).
        return BlockProcessResult::kRevisitLoop;
      } else {
        // Discarding temporary snapshot.
        if (!field_values().IsSealed()) field_values().Seal();
        // Discarding the backedge's snapshot, so that if we're currently in a
        // nested loop, we can still revisit it if we revisit the outer loop.
        block_snapshots_.erase(block);
      }
    }

    return BlockProcessResult::kContinue;
  }

  void PatchLoopPhisBackedges(BasicBlock* header, Snapshot backedge_snapshot) {
    if (!new_phis().contains(header)) return;

    // TODO(dmercadier): introduce a notion of "temporary read-only snapshot" in
    // the SnapshotTable, and start such a temporary read-only snapshot here.
    // This would be similar to the TemporaryVariableSnapshots that the
    // VariableReducer creates in Turboshaft.
    field_values().StartNewSnapshot({backedge_snapshot});

    int backedge_index = header->predecessor_count() - 1;
    for (auto& [phi, key] : *new_phis().at(header)) {
      ValueNode* backedge_val = field_values().Get(key);
      DCHECK_NOT_NULL(backedge_val);
      TRACE(">> Updating loop phi backedge: "
            << PRINT_NODE(phi) << " backedge "
            << PRINT_NODE(phi->input(backedge_index).node()) << " -> "
            << PRINT_NODE(backedge_val));
      phi->change_input(backedge_index, backedge_val);
    }

    field_values().Seal();
  }

  // Returns true if a candidate for eliding flows into a loop phi. In that
  // case, we'll invalidate it and reprocess the loop.
  // TODO(dmercadier): try to merge objects in that case, instead of
  // invalidating.
  bool CheckLoopPhiInvalidation(BasicBlock* loop_header) {
    TRACE("CheckLoopPhiInvalidation");
    if (!loop_header->has_phi()) {
      TRACE("> no phis");
      return false;
    }

    bool invalidated = false;
    for (Phi* phi : *loop_header->phis()) {
      if (InlinedAllocation* alloc =
              data_.TryGetCandidateInlinedAllocation(phi->backedge_input())) {
        TRACE("> Marking " << NODE_ID(alloc) << " as escaping, will revisit");
        data_.MarkAsEscaped(alloc);
        invalidated = true;
      }
    }
    return invalidated;
  }

  ProcessResult Process(AssertEscapeAnalysisElided* node,
                        const ProcessingState& state) {
    return CandidateAnalyzer::Process(node, state);
  }

  // Phis don't go through the generic NodeT overload below because we don't
  // want to create new snapshots for each Phi, because this will prevent using
  // GetPredecessorAt, which we need in order to update Phi inputs that depend
  // on elided values in predecessors.
  ProcessResult Process(Phi* node, const ProcessingState& state) {
    return CandidateAnalyzer::Process(node, state);
  }

  template <class NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    // We're recording a new snapshot for every node so that the Elider can know
    // the current value of any elided field at any point.
    // TODO(dmercadier): this is recording more snapshots than we strictly need
    // to, given that we only need snapshots for:
    //
    //   - deopting nodes, to be able to patch the DeoptFrame.
    //
    //   - stores and loads to be able to figure out if their base has been
    //     elided, in which case we'll remove them (stores) or replace them with
    //     identities to the current field value (loads).
    //
    // For all other nodes, we shouldn't need a snapshot.
    if (!field_values().IsSealed()) {
      DCHECK(!field_values().IsSealed());
      Snapshot snapshot = field_values().Seal();
      snapshots().insert_or_assign(node, snapshot);

      field_values().StartNewSnapshot(snapshot);
    }

    return CandidateAnalyzer::Process(node, state);
  }

  void RegisterNewPhi(Phi* phi, BasicBlock* block, Key key) {
    auto [it, inserted] = new_phis().try_emplace(block, nullptr);
    if (inserted) {
      it->second = zone()->New<ZoneVector<std::pair<Phi*, Key>>>(zone());
    }
    it->second->push_back({phi, key});
  }

  Zone* zone() { return data_.zone; }
  FieldValuesTable& field_values() { return data_.field_values; }
  ZoneAbslFlatHashMap<NodeBase*, Snapshot>& snapshots() {
    return data_.snapshots;
  }
  ZoneAbslFlatHashMap<ObjectField, Key>& keys_mappings() {
    return data_.keys_mappings;
  }
  ZoneAbslFlatHashMap<BasicBlock*, ZoneVector<std::pair<Phi*, Key>>*>&
  new_phis() {
    return data_.new_phis;
  }
  MaglevGraphLabeller* labeller() { return data_.labeller(); }

 private:
  EscapeAnalysisData& data_;
  ZoneAbslFlatHashMap<BasicBlock*, MaybeSnapshot> block_snapshots_;
  Tracer tracer_;
};

void EscapeAnalysis::AnalyzeCandidates() {
  TRACE("EscapeAnalysis::AnalyzeCandidates");
  GraphProcessor<FieldValuesTracker> processor(data_);
  processor.ProcessGraph(data_.graph);

#ifdef DEBUG
  if (v8_flags.trace_turbolev_escape_analysis) {
    StdoutStream() << "\nAfter AnalyzeCandidates; candidates are:\n";
    for (auto candidate : data_.candidates) {
      if (candidate.second != CandidateStatus::kCannotElide) {
        StdoutStream() << "  - " << PRINT_NODE(candidate.first) << "\n";
      }
    }
    StdoutStream() << "\n\n";
  }
#endif
}

bool EscapeAnalysis::HasElidableCandidates() const {
  for (auto candidate : data_.candidates) {
    if (candidate.second != CandidateStatus::kCannotElide) {
      return true;
    }
  }
  return false;
}

// Updating DeoptFrames is done by the DeoptFrameUpdater rather than by the
// Elider in order to make it hard to forget to update the DeoptFrame of a given
// node: the Elider has a lot of Process overloads for various node types, and
// so it would be easy to forget to update a DeoptFrame in one of those
// overloads. By contrast, the DeoptFrameUpdater only has a single Process
// method, which just updates DeoptFrames, and so it's not possible to forget to
// process a given node type.
class DeoptFrameUpdater {
 public:
  explicit DeoptFrameUpdater(EscapeAnalysisData& data)
      : data_(data), tracer_(data.compilation_info) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

  template <class NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    DCHECK_IMPLIES(node->properties().can_eager_deopt(),
                   NodeT::kProperties.can_eager_deopt());
    DCHECK_IMPLIES(node->properties().can_lazy_deopt(),
                   NodeT::kProperties.can_lazy_deopt());

    // Updating deopt state.
    // TODO(dmercadier): we currently clone all DeoptInfo without even checking
    // if they have elided object inputs that would warrant cloning them. We
    // should instead first inspect the DeoptInfo to see if it has any input
    // that needs to be updated, and only then clone it.
    if constexpr (NodeT::kProperties.can_eager_deopt()) {
      TRACE("> Will UpdateEagerDeoptInfo");
      UpdateEagerDeoptInfo(node);
    }

    if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      TRACE("> Will UpdateLazyDeoptInfo");
      UpdateLazyDeoptInfo(node);
    }

    return ProcessResult::kContinue;
  }

  template <class NodeT>
  void UpdateEagerDeoptInfo(NodeT* node) {
    static_assert(NodeT::kProperties.can_eager_deopt());
    DeoptFrame* new_frame_state =
        DeepClone(node->eager_deopt_info()->top_frame(), zone());
    node->SetEagerDeoptInfo(zone(), new_frame_state,
                            node->eager_deopt_info()->feedback_to_update());
    UpdateDeoptFrame(new_frame_state);
  }

  template <class NodeT>
  void UpdateLazyDeoptInfo(NodeT* node) {
    static_assert(NodeT::kProperties.can_lazy_deopt());
    DeoptFrame* new_frame_state =
        DeepClone(node->lazy_deopt_info()->top_frame(), zone());

    interpreter::Register result_location =
        interpreter::Register::virtual_accumulator();
    int result_size = 1;
    switch (node->lazy_deopt_info()->top_frame().type()) {
      case DeoptFrame::FrameType::kInterpretedFrame:
        // Interpreted frames obviously need a result location.
        result_location = node->lazy_deopt_info()->result_location();
        result_size = node->lazy_deopt_info()->result_size();
        break;
      case DeoptFrame::FrameType::kInlinedArgumentsFrame:
      case DeoptFrame::FrameType::kConstructInvokeStubFrame:
        break;
      case DeoptFrame::FrameType::kBuiltinContinuationFrame:
        // Normally if the function is going to be deoptimized then the top
        // frame should be an interpreted one, except for LazyDeoptContinuation
        // builtin.
        switch (node->lazy_deopt_info()
                    ->top_frame()
                    .as_builtin_continuation()
                    .builtin_id()) {
          case Builtin::kGenericLazyDeoptContinuation:
          case Builtin::kGetIteratorWithFeedbackLazyDeoptContinuation:
          case Builtin::kCallIteratorWithFeedbackLazyDeoptContinuation:
            result_location = node->lazy_deopt_info()->result_location();
            result_size = node->lazy_deopt_info()->result_size();
            break;
          default:
            break;
        }
    }

    node->SetLazyDeoptInfo(zone(), new_frame_state, result_location,
                           result_size,
                           node->lazy_deopt_info()->feedback_to_update());
    UpdateDeoptFrame(new_frame_state);
  }

 private:
  void UpdateDeoptFrame(DeoptFrame* frame) {
    // TODO(dmercadier): We should only compute the current state of
    // VirtualObjects that are used in the current DeoptFrame, rather than
    // computing it for all VirtualObjects.
    // TODO(dmercadier): Optimize shared DeoptFrame cloning.
    // Currently, if a DeoptFrame is shared by multiple nodes in the graph,
    // we clone it for every reference. We could instead reuse a previously
    // cloned frame if the state (field values) of the virtual objects inside
    // it has not changed since it was cloned.
    // This is non-trivial because it requires tracking a "version" or "epoch"
    // for virtual object states to detect if any updates occurred between uses.
    VirtualObjectList virtual_objects;
    std::unordered_map<ValueNode*, VirtualObject*> vobj_map;

    // New VirtualObjects are created in 2 steps: the first one allocates the
    // VirtualObjects and doesn't set any fields, while the second one sets
    // their fields. This is needed to handle recursive (and in particular
    // mutually recursive) objects.

    // Step 1: allocating the virtual object.
    TRACE("> UpdateDeoptFrame. Step 1, allocating Vobjs");
    int next_id = 0;
    for (auto& [node, status] : candidates()) {
      if (status == CandidateStatus::kCannotElide) continue;
      TRACE(">> id=" << next_id << " candidate=" << PRINT_NODE(node));
      VirtualObject* existing_vobj = node->object();
      int field_count = existing_vobj->slot_count();
      const vobj::ObjectLayout* object_layout = existing_vobj->object_layout();
      TRACE(">>> field_count=" << field_count << " (byte_size="
                               << existing_vobj->size() << ")");
      ValueNode** slots =
          zone()->template AllocateArray<ValueNode*>(field_count);
      if (!data_.HasMapFor(node)) {
        // This vobj cannot be live at this point of the program.
        // TODO(dmercadier): if we only snapshot VirtualObjects that are
        // required by the current DeoptFrame, we shouldn't need this bailout.
        continue;
      }
      auto maps = data_.GetMaps(node);
      DCHECK(!maps.empty());
      VirtualObject* vobj = NodeBase::New<VirtualObject>(
          zone(), 0, maps[0], next_id, object_layout, field_count, slots);

#ifdef V8_ENABLE_MAGLEV_GRAPH_PRINTER
      labeller()->RegisterNode(vobj);
#endif

      TRACE(">>> new Vobj: " << vobj);
      next_id++;
      vobj_map.insert({node, vobj});
    }

    // Step 2: setting the field values.
    TRACE("> UpdateDeoptFrame. Step 2: initializing Vobjs");
    for (auto& [node, status] : candidates()) {
      if (status == CandidateStatus::kCannotElide) continue;
      TRACE(">> id=" << next_id << " candidate=" << PRINT_NODE(node));
      if (!vobj_map.contains(node)) continue;
      VirtualObject* vobj = vobj_map.at(node);
      TRACE(">>> vobj id:" << NODE_ID(vobj));
      int field_count = vobj->slot_count();
      bool continue_outer = false;
      for (int i = 0; i < field_count; i++) {
        int offset = vobj->FieldForSlot(i).offset;
        ValueNode* field_value = data_.Get(node, offset);
        if (field_value == nullptr) {
          // TODO(dmercadier): if we only snapshot VirtualObjects that are
          // required by the current DeoptFrame, we shouldn't need this bailout.
          TRACE(">>>> Skipping this vobj: currently not live");
          continue_outer = true;
          break;
        }

        TRACE(">>> vobj_id=" << NODE_ID(vobj) << " offset=" << offset
                             << " value=" << NODE_ID(field_value));

        vobj->set(offset, field_value);
      }
      if (continue_outer) continue;

      vobj->set_allocation(node);
      // Now that we're sure that {vobj} is alive and we've initialized all of
      // its field, so we can add it to {virtual_objects}.
      virtual_objects.Add(vobj);
    }

    UpdateVirtualObjects(frame, virtual_objects);
    UpdateFrameState(frame, vobj_map, data_);
  }

 private:
  Zone* zone() { return data_.zone; }
  Candidates& candidates() { return data_.candidates; }
  FieldValuesTable& field_values() { return data_.field_values; }
  ZoneAbslFlatHashMap<NodeBase*, Snapshot>& snapshots() {
    return data_.snapshots;
  }
  ZoneAbslFlatHashMap<ObjectField, Key>& keys_mappings() {
    return data_.keys_mappings;
  }
  MaglevGraphLabeller* labeller() { return data_.labeller(); }

  EscapeAnalysisData& data_;
  Tracer tracer_;
};

class Elider {
 public:
  explicit Elider(EscapeAnalysisData& data)
      : data_(data), tracer_(data.compilation_info) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    // New phis are only inserted after {block} has been processed so that they
    // aren't visited.

    TRACE("PostProcessBasicBlock " << BLOCK_ID(block));
    if (data_.new_phis.contains(block)) {
      TRACE("> Inserting new phis");
      DCHECK(block->has_state());
      for (auto& [phi, key] : *data_.new_phis.at(block)) {
        block->AddPhi(phi);
      }
    }

    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

  ProcessResult Process(InlinedAllocation* node, const ProcessingState&) {
    if (candidates().contains(node) &&
        candidates().at(node) != CandidateStatus::kCannotElide) {
      TRACE("Eliding " << PRINT_NODE(node));
      node->SetElided();
      return ProcessResult::kRemove;
    } else {
      TRACE("NOT eliding " << PRINT_NODE(node));
    }
    return ProcessResult::kContinue;
  }

  ProcessResult RemoveIfElidedBase(ValueNode* base, Node* node) {
    TRACE("Process " << PRINT_NODE(node));
    if (data_.TryGetCandidateInlinedAllocation(base)) {
      TRACE("> Removing because base has been elided");
      return ProcessResult::kRemove;
    }
    TRACE("> Keeping");
    return ProcessResult::kContinue;
  }

  ProcessResult Process(StoreMap* node, const ProcessingState&) {
    return RemoveIfElidedBase(node->ValueInput().node(), node);
  }

  ProcessResult Process(StoreTaggedFieldNoWriteBarrier* node,
                        const ProcessingState&) {
    return RemoveIfElidedBase(node->ObjectInput().node(), node);
  }

  ProcessResult Process(StoreTaggedFieldWithWriteBarrier* node,
                        const ProcessingState&) {
    return RemoveIfElidedBase(node->ObjectInput().node(), node);
  }

  ProcessResult Process(StoreFixedArrayElementNoWriteBarrier* node,
                        const ProcessingState&) {
    return RemoveIfElidedBase(node->ElementsInput().node(), node);
  }

  ProcessResult Process(StoreFixedArrayElementWithWriteBarrier* node,
                        const ProcessingState&) {
    return RemoveIfElidedBase(node->ElementsInput().node(), node);
  }

  ProcessResult Process(StoreFixedDoubleArrayElement* node,
                        const ProcessingState&) {
    return RemoveIfElidedBase(node->ElementsInput().node(), node);
  }

  ProcessResult Process(StoreInt32* node, const ProcessingState&) {
    return RemoveIfElidedBase(node->ObjectInput().node(), node);
  }

  ProcessResult Process(StoreFloat64* node, const ProcessingState&) {
    return RemoveIfElidedBase(node->ObjectInput().node(), node);
  }

  ProcessResult Process(AssertEscapeAnalysisElided* node,
                        const ProcessingState&) {
    TRACE("Process AssertEscapeAnalysisElided in Elider: " << PRINT_NODE(node));
    ValueNode* input = node->input(0).node();
    if (data_.TryGetCandidateInlinedAllocation(input)) {
      TRACE("> Removing because input has been elided");
      return ProcessResult::kRemove;
    }
    FATAL(
        "Escape analysis assertion failed: %%AssertEscapeAnalysisElided input "
        "escaped or was not elided!");
  }

  template <typename CheckMapsT>
  ProcessResult ProcessCheckMaps(CheckMapsT* node) {
    TRACE("Process " << PRINT_NODE(node));
    if (data_.TryGetCandidateInlinedAllocation(node->ReceiverInput().node())) {
      TRACE("> vobj must have the right map, so removing CheckMaps");
      return ProcessResult::kRemove;
    }
    TRACE("> base not elided");

    return ProcessResult::kContinue;
  }
  ProcessResult Process(CheckMaps* node, const ProcessingState&) {
    return ProcessCheckMaps(node);
  }
  ProcessResult Process(CheckMapsWithMigration* node, const ProcessingState&) {
    return ProcessCheckMaps(node);
  }
  ProcessResult Process(CheckMapsWithMigrationAndDeopt* node,
                        const ProcessingState&) {
    return ProcessCheckMaps(node);
  }

  ProcessResult Process(AssumeMap* node, const ProcessingState&) {
    if (data_.TryGetCandidateInlinedAllocation(node->ObjectInput().node())) {
      TRACE("> Input has been removed, so removing AssumeMaps");
      return ProcessResult::kRemove;
    }
    return ProcessResult::kContinue;
  }

  template <typename NodeT>
  ProcessResult ProcessLoad(NodeT* node, ValueNode* base, int offset) {
    TRACE("ProcessLoad for " << PRINT_NODE(node));
    if (InlinedAllocation* alloc =
            data_.TryGetCandidateInlinedAllocation(base)) {
      TRACE("> will remove because base is candidate: " << PRINT_NODE(alloc));
      ObjectField addr = ObjectField{alloc, offset};
      if (!keys_mappings().contains(addr)) {
        // All fields should be initialized. If this field isn't initialized, it
        // means that this node is unreachable.
        // TODO(dmercadier): emit an Abort and terminate the current block. For
        // now I just replace this unreachable node with an identity to a
        // Constant for simplicity.
        ValueNode* dummy_cst;
        switch (node->value_representation()) {
          case ValueRepresentation::kTagged:
            dummy_cst = data_.graph->GetSmiConstant(0);
            break;
          case ValueRepresentation::kInt32:
            dummy_cst = data_.graph->GetInt32Constant(0);
            break;
          case ValueRepresentation::kUint32:
            dummy_cst = data_.graph->GetUint32Constant(0);
            break;
          case ValueRepresentation::kFloat64:
            dummy_cst = data_.graph->GetFloat64Constant(0);
            break;
          case ValueRepresentation::kHoleyFloat64:
            dummy_cst = data_.graph->GetFloat64Constant(0);
            break;
          case ValueRepresentation::kIntPtr:
            dummy_cst = data_.graph->GetIntPtrConstant(0);
            break;
          case ValueRepresentation::kRawPtr:
          case ValueRepresentation::kNone:
            UNREACHABLE();
        }
        node->OverwriteWithIdentityTo(dummy_cst);
        return ProcessResult::kContinue;
      }
      DCHECK(keys_mappings().contains(addr));
      Key key = keys_mappings().at(addr);
      ValueNode* replacement = field_values().Get(key);
      DCHECK_NOT_NULL(replacement);

      if (!IsStillInTheGraph(replacement)) {
        // {replacement} isn't in the graph anymore, so we don't overwrite the
        // current node with an Identity to it. It might be tempting to kill the
        // current node (with a ClearInput and OverwriteWith<Dead>), but this
        // would lead to issues down the line since it can still have uses that
        // will want to inspect it to know what to do (for instance, if it's
        // used in an AssumeMap, the AssumeMap will look at what this node is in
        // order to know if it should be elided or not).
        TRACE("> Actually removing");
        return ProcessResult::kRemove;
      }

      TRACE("> overwriting with Identity");
      node->template OverwriteWith<Identity>();
      node->change_input(0, replacement);
    }

    return ProcessResult::kContinue;
  }

  maglev::ProcessResult Process(maglev::LoadTaggedField* node,
                                const maglev::ProcessingState& state) {
    return ProcessLoad(node, node->ValueInput().node(), node->offset());
  }

  maglev::ProcessResult Process(maglev::LoadFloat64* node,
                                const maglev::ProcessingState& state) {
    return ProcessLoad(node, node->ValueInput().node(), node->offset());
  }

  maglev::ProcessResult Process(maglev::LoadFixedDoubleArrayElement* node,
                                const maglev::ProcessingState& state) {
    if (Int32Constant* cst =
            node->IndexInput().node()->TryCast<Int32Constant>()) {
      int offset = FixedDoubleArray::OffsetOfElementAt(cst->value());
      return ProcessLoad(node, node->ElementsInput().node(), offset);
    }

    return ProcessResult::kContinue;
  }

  ProcessResult Process(LoadFixedArrayElement* node,
                        const ProcessingState& state) {
    // This overload doesn't do anything special but is needed in order to keep
    // the overloads in sync with the CandidateAnalyzer.
    return Process(static_cast<NodeBase*>(node), state);
  }

  bool IsStillInTheGraph(ValueNode* node) {
    if (data_.TryGetCandidateInlinedAllocation(node)) {
      // Elided InlinedAllocations are removed from the graph.
      return false;
    }
    // All other ValueNodes stay in the graph, including Loads from elided
    // InlinedAllocations, which are overwritten by Identity.
    return true;
  }

  // We constant-fold BranchIfReferenceEqual if either its left or right input
  // is an elided object.
  ProcessResult Process(BranchIfReferenceEqual* node, const ProcessingState&) {
    InlinedAllocation* left_alloc =
        data_.TryGetCandidateInlinedAllocation(node->LeftInput().node());
    InlinedAllocation* right_alloc =
        data_.TryGetCandidateInlinedAllocation(node->RightInput().node());

    // TODO(dmercadier): when folding this branch, we should also mark the
    // non-taken side as Unreachable (eg, by calling RemovePredecessorFollowing
    // and set_may_have_unreachable_blocks).
    if (left_alloc) {
      if (right_alloc) {
        // Both left and right inputs were elided, checking if they are the same
        // object or not.
        BasicBlock* target =
            left_alloc == right_alloc ? node->if_true() : node->if_false();
        node->OverwriteWith<Jump>();
        node->Cast<Jump>()->set_target(target);
      } else {
        // Left was elided but right wasn't ==> they can't be the same object.
        BasicBlock* target = node->if_false();
        node->OverwriteWith<Jump>();
        node->Cast<Jump>()->set_target(target);
      }
    } else if (right_alloc) {
      // Right was elided but left wasn't ==> they can't be the same object.
      BasicBlock* target = node->if_false();
      node->OverwriteWith<Jump>();
      node->Cast<Jump>()->set_target(target);
    }

    return ProcessResult::kContinue;
  }

  ProcessResult Process(Phi* phi, const ProcessingState&) {
    // Nothing to do since we don't elide nodes that flow into Phis (yet).
    return ProcessResult::kContinue;
  }

  template <class NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    TRACE("Nothing to do for regular node " << PRINT_NODE(node));

    // TODO(dmercadier): it would be nice to clear unused nodes, but it's not
    // necessarily trivial, since for instance a HeapConstant could be unused
    // when processed in the Process(HeapConstant*) call but then we could add a
    // use for it when emitting a FrameState later on (if it's a Map for
    // instance).

    return ProcessResult::kContinue;
  }

 public:
  Zone* zone() { return data_.zone; }
  Candidates& candidates() { return data_.candidates; }
  FieldValuesTable& field_values() { return data_.field_values; }
  ZoneAbslFlatHashMap<NodeBase*, Snapshot>& snapshots() {
    return data_.snapshots;
  }
  ZoneAbslFlatHashMap<ObjectField, Key>& keys_mappings() {
    return data_.keys_mappings;
  }
  MaglevGraphLabeller* labeller() { return data_.labeller(); }

  EscapeAnalysisData& data_;
  Tracer tracer_;
};

// The main part of the eliding is done by the Elider processor, but it's called
// through EliderWrapper which takes care of opening and closing memory
// snapshots.
class FieldValuesRestorer {
 public:
  explicit FieldValuesRestorer(EscapeAnalysisData& data)
      : data_(data), tracer_(data.compilation_info) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

  template <class NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    if (snapshots().contains(node)) {
      if (!field_values().IsSealed()) field_values().Seal();
      DCHECK(snapshots().contains(node));
      Snapshot snapshot = snapshots().at(node);
      field_values().StartNewSnapshot(snapshot);
    }

    return ProcessResult::kContinue;
  }

 private:
  FieldValuesTable& field_values() { return data_.field_values; }
  ZoneAbslFlatHashMap<NodeBase*, Snapshot>& snapshots() {
    return data_.snapshots;
  }
  MaglevGraphLabeller* labeller() { return data_.labeller(); }

  EscapeAnalysisData& data_;
  Tracer tracer_;
};

void EscapeAnalysis::ElideCandidates() {
  TRACE("\n\nEscapeAnalysis::ElideCandidates");

  // This phase will update some deopt frames, thus rendering the recorded deopt
  // frames in the graph stale. We thus clear them now.
  // Note that we don't repopulate eager_deopt_top_frames_ and
  // lazy_deopt_top_frames_ in the graph since the Turbolev pipeline with this
  // escape analysis will never read this again. In particular,
  // UnwrapDeoptFrames (which is the main place where those frames are read)
  // doesn't handle recursive (and mutually recursive) objects, and adding
  // support for this would add a significant cost with no obvious benefits, so
  // we just avoid calling UnwrapDeoptFrames.
  // TODO(dmercadier): handle recursive VirtualObjects in UnwrapDeoptFrames and
  // start recording DeoptFrames when we create them.
  data_.graph->ClearRecordedFrames();

  // The order of the processors in the following GraphMultiProcessor is
  // important:
  //
  //   1. FieldValuesRestorer must be first since it's taking care of restoring
  //      the Snapshots with the right field values for the current node.
  //      Without this, the Elider and DeoptFrameUpdater would update on stale
  //      field values.
  //
  //   2. Elider must be before DeoptFrameUpdater because it could remove nodes,
  //      which would mean that there would be no need to update their
  //      DeoptFrames (and thus no need to process them with the
  //      DeoptFrameUpdater).
  //
  GraphMultiProcessor<FieldValuesRestorer, Elider, DeoptFrameUpdater> processor(
      FieldValuesRestorer{data_}, Elider{data_}, DeoptFrameUpdater{data_});
  processor.ProcessGraph(data_.graph);
}

namespace {
// The CandidateAnalyzer and the Elider must always be in sync. In particular,
// if the CandidateAnalyzer has special handling for a node, then the Elider
// should also have special handling for it. For instance, if the
// CandidateAnalyzer allows the array input of LoadFixedDoubleArrayElement to be
// elided, but the Elider doesn't, then the Elider will not remove this from the
// graph and we'll be left with a node with a stale input.
// This is where HasSpecificProcess comes in: it checks if the Elider or the
// CandidateAnalyzer has a specific Process overload, then the other one as
// well.

template <typename T, typename NodeT>
constexpr bool HasSpecificProcess() {
  using Signature = ProcessResult (T::*)(NodeT*, const ProcessingState&);
  // If T has a specific non-template Process(NodeT*, ...) overload,
  // the static_cast selects it, resulting in a different address than
  // the template instantiation.
  return static_cast<Signature>(&T::Process) != &T::template Process<NodeT>;
}

// Static assertion check for a single node type
#define CHECK_SYNCHRONIZED_OVERLOAD(NodeT)                                 \
  static_assert(HasSpecificProcess<CandidateAnalyzer, NodeT>() ==          \
                    HasSpecificProcess<Elider, NodeT>(),                   \
                "CandidateAnalyzer and Elider must be synchronized: both " \
                "must either overload or not overload Process(" #NodeT "*)");

// Run the check for all Maglev node types automatically
NODE_BASE_LIST(CHECK_SYNCHRONIZED_OVERLOAD)
#undef CHECK_SYNCHRONIZED_OVERLOAD

}  // namespace

}  // namespace v8::internal::maglev
