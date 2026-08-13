// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/turbolev-escape-analysis.h"

#include <optional>

#include "absl/container/flat_hash_set.h"
#include "src/base/container-utils.h"
#include "src/base/iterator.h"
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
      auto it = loaded_values.find(load);
      if (it != loaded_values.end()) return it->second;
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
    ValueNode* val = predecessor_index == -1 ? field_values.Get(key)
                                             : field_values.GetPredecessorValue(
                                                   key, predecessor_index);
    if (val == nullptr) {
      // The key is valid, but the value is nullptr (e.g. because it is
      // uninitialized on this path, or merged to nullptr due to predecessor
      // mismatch). For now, we also just mark {alloc} as escaping when this
      // happens.
      // TODO(dmercadier): We should also avoid marking {alloc} as escaping
      // here. We could insert a special marker in the graph and in the Elider
      // mark this branch as Unreachable.
      MarkAsEscaped(alloc);
      return fallback;
    }
    return val;
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
  if (HasEscaped(alloc)) return;
  TRACE("Marking candidate:" << NODE_ID(alloc) << " as escaping");
  if (!(alloc->HasBeenAnalysed() && alloc->HasEscaped())) alloc->SetEscaped();
  candidates.at(alloc) = CandidateStatus::kCannotElide;

  // Trigger revisits
  DCHECK(alloc_definition_loop.contains(alloc));
  BasicBlock* defining_loop = alloc_definition_loop.at(alloc);
  for (auto& loop : base::Reversed(loop_stack)) {
    if (loop.header == defining_loop) break;
    loop.has_escaped_candidate = true;
  }

  if (alloc_dependencies.contains(alloc)) {
    for (InlinedAllocation* other : *alloc_dependencies.at(alloc)) {
      if (candidates.contains(other) &&
          candidates.at(other) != CandidateStatus::kCannotElide) {
        MarkAsEscaped(other);
      }
    }
  }
}

bool EscapeAnalysisData::HasEscaped(InlinedAllocation* alloc) {
  return candidates.at(alloc) == CandidateStatus::kCannotElide;
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

ValueNode* GetUpdatedValueAndAddDeoptUse(
    ValueNode* value,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map,
    EscapeAnalysisData& data) {
  ValueNode* resolved = data.ResolveBase(value);

  // We didn't increment use-count when cloning the DeoptFrame (because the
  // VirtualObjects weren't ready yet), so we need to do it now.
  // TODO(dmercadier): we can't decrement use-count of the original DeoptFrame
  // because it could be cloned multiple times, which would lead to wrongly
  // decrementing multiple times. Figure out a way to get accurate use-counts
  // after escape analysis.
  resolved->add_use();

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
  frame.closure() =
      GetUpdatedValueAndAddDeoptUse(frame.closure(), vobj_map, data);

  frame.frame_state()->ForEachValue(
      frame.unit(),
      [&vobj_map, &data](ValueNode*& value, interpreter::Register owner) {
        value = GetUpdatedValueAndAddDeoptUse(value, vobj_map, data);
      });
}

void UpdateInlinedArgumentsFrame(
    InlinedArgumentsDeoptFrame& frame,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map,
    EscapeAnalysisData& data) {
  frame.closure() =
      GetUpdatedValueAndAddDeoptUse(frame.closure(), vobj_map, data);

  for (ValueNode*& value : frame.arguments()) {
    value = GetUpdatedValueAndAddDeoptUse(value, vobj_map, data);
  }
}

void UpdateConstructInvokeStubFrame(
    ConstructInvokeStubDeoptFrame& frame,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map,
    EscapeAnalysisData& data) {
  // Note that while the receiver cannot be elided, it could still be a
  // LoadTaggedField from an elided base, in which case it still need to be
  // updated to bypass the load.
  frame.receiver() =
      GetUpdatedValueAndAddDeoptUse(frame.receiver(), vobj_map, data);
  frame.context() =
      GetUpdatedValueAndAddDeoptUse(frame.context(), vobj_map, data);
}

void UpdateBuiltinContinuationFrame(
    BuiltinContinuationDeoptFrame& frame,
    const std::unordered_map<ValueNode*, VirtualObject*>& vobj_map,
    EscapeAnalysisData& data) {
  frame.context() =
      GetUpdatedValueAndAddDeoptUse(frame.context(), vobj_map, data);

  for (ValueNode*& value : frame.parameters()) {
    value = GetUpdatedValueAndAddDeoptUse(value, vobj_map, data);
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

// The AnalyzerPrePass is a simple straightword pass over the graph whose goal
// is to:
//
//    1. Figure out whether any allocation is not guaranteed to escape. If we
//       find such an allocation, then EscapeAnalysis::AnalyzeCandidates will
//       run the CandidateAnalyzer which will do more expensive state tracking
//       and loop fixpoints to figure precisely what can definitely be elided
//       and how.
//
//    2. Figure out which allocations are guaranteed to eventually escape. The
//       CandidateAnalyzer can then ignore them, thus saving memory (no need to
//       track their fields) and time (no need to revisit loops because of
//       them).
class AnalyzerPrePass {
 public:
  explicit AnalyzerPrePass(EscapeAnalysisData& data)
      : data_(data), tracer_(data.compilation_info) {}
  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    if (block->is_exception_handler_block()) {
      data_.has_exception_handler = true;
    }
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

// List of nodes that might not make their inputs escape. This list should be
// kept in sync with the overloads in the CandidateAnalyzer and in the Elider;
// HasSpecificProcess at the end of this file will trigger a static_assert if
// this list falls out of sync.
#define NOT_ESCAPING_OPS(V)                 \
  V(StoreMap)                               \
  V(StoreTaggedFieldNoWriteBarrier)         \
  V(StoreTaggedFieldWithWriteBarrier)       \
  V(StoreFixedArrayElementNoWriteBarrier)   \
  V(StoreFixedArrayElementWithWriteBarrier) \
  V(StoreFixedDoubleArrayElement)           \
  V(StoreInt32)                             \
  V(StoreFloat64)                           \
  V(AssertEscapeAnalysisElided)             \
  V(CheckMaps)                              \
  V(CheckMapsWithMigration)                 \
  V(CheckMapsWithMigrationAndDeopt)         \
  V(AssumeMap)                              \
  V(LoadTaggedField)                        \
  V(LoadFloat64)                            \
  V(LoadFixedDoubleArrayElement)            \
  V(LoadFixedArrayElement)                  \
  V(BranchIfReferenceEqual)

#define PROCESS_NOT_ESCAPING(Op)                               \
  ProcessResult Process(maglev::Op*, const ProcessingState&) { \
    return ProcessResult::kContinue;                           \
  }
  NOT_ESCAPING_OPS(PROCESS_NOT_ESCAPING)
#undef PROCESS_NOT_ESCAPING
#undef NOT_ESCAPING_OPS

  ProcessResult Process(InlinedAllocation* node, const ProcessingState&) {
    auto [it, inserted] =
        data_.candidates.try_emplace(node, CandidateStatus::kCanMaybeElide);
    if (!inserted) {
      // If {node} is already recorded, it must be because it got marked as
      // CannotElide as the backedge of a loop phi (since otherwise we always
      // visit definitions before uses).
      DCHECK_EQ(it->second, CandidateStatus::kCannotElide);
      DCHECK(node->HasEscaped());
    }
    return ProcessResult::kContinue;
  }

  ProcessResult Process(Phi* node, const ProcessingState& state) {
    // Phis make their input escape but need a special overload in order to keep
    // the overloads in this class in sync with the CandidateAnalyzer and the
    // Elider (cf the HasSpecificProcess check).
    return Process(static_cast<NodeBase*>(node), state);
  }

  template <class NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState&) {
    // Any operation that isn't whitelisted in NOT_ESCAPING_OPS is assumed to
    // make its inputs escape.
    for (Input input : node->inputs()) {
      if (InlinedAllocation* alloc =
              input.node()->template TryCast<InlinedAllocation>()) {
        data_.candidates[alloc] = CandidateStatus::kCannotElide;
        if (!alloc->HasBeenAnalysed()) {
          alloc->SetEscaped();
        } else {
          DCHECK(alloc->HasEscaped());
        }
      }
    }
    return ProcessResult::kContinue;
  }

 private:
  EscapeAnalysisData& data_;
  Tracer tracer_;
};

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
      int index = index_node->value();
      // Note that in unreachable code, {index} could be negative or too large,
      // but this shouldn't be possible here, since the GraphOptimizer should
      // truncate the graph in such cases.
      CHECK(index >= 0 &&
            static_cast<uint32_t>(index) <= FixedArrayT::kMaxLength);

      int offset = FixedArrayT::OffsetOfElementAt(index);
      DCHECK_NE(offset, offsetof(FixedArrayT, map_));

      ProcessFieldStore(node, node->ElementsInput().node(), offset,
                        node->ValueInput().node());
      return ProcessResult::kContinue;
    }

    // TODO(dmercadier): handle non-constant indices. This will require
    // stack-allocating the array.
    data_.MarkAsEscapedIfCandidate(node->ElementsInput().node());
    data_.MarkAsEscapedIfCandidate(node->ValueInput().node());

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
    ValueNode* resolved =
        data_.ResolveLoadBase(node->ValueInput(), node->offset(), node);
    data_.loaded_values.insert_or_assign(node, resolved);
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
    // When visiting a loop header for the 1st time, the backedge predecessor
    // has no snapshot yet, and thus no value to look up: its inputs are handled
    // by MarkEscapingLoopPhiBackedges when reaching the JumpLoop instead.
    DCHECK_LE(data_.merged_predecessor_count, phi->input_count());
    for (int i = 0; i < data_.merged_predecessor_count; i++) {
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
    // TODO(dmercadier): consider running proper DCE before escape analysis
    // to not block escape analysis on dead nodes.

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
        old_phis_(data.zone),
        tracer_(data.compilation_info) {}

  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    TRACE("PreProcessBasicBlock " << BLOCK_ID(block));
    if (block->is_loop()) {
      data_.loop_stack.push_back({block, false});
    }
    old_phis_.clear();

    auto new_phis_it = new_phis().find(block);
    if (new_phis_it != new_phis().end()) {
      // We are revisiting this block because of a loop.
      TRACE("> clearing previous new Phis");
      if (block->is_loop()) {
        old_phis_.swap(*new_phis_it->second);
      } else {
        new_phis_it->second->clear();
      }
    }

    // The PrePass should make escape analysis bail on exception handlers.
    DCHECK(!block->is_exception_handler_block());

    CreateSnapshotFor(block);

    return BlockProcessResult::kContinue;
  }

  bool CreateSnapshotFor(BasicBlock* block,
                         bool is_loop_fixpoint_check = false) {
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
    data_.merged_predecessor_count = predecessor_count;

    std::optional<MergePointInterpreterFrameState*> state;
    DCHECK_IMPLIES(predecessor_count > 1, block->has_state());
    if (block->has_state()) {
      state = block->state();
    }

    bool need_revisit = false;
    auto merge_field_values =
        [&](Key key,
            base::Vector<ValueNode* const> predecessors) -> ValueNode* {
      DCHECK_NOT_NULL(key.data().base);
      if (data_.HasEscaped(key.data().base)) {
        // No need to bother introducing phis since this allocation has escaped.
        return nullptr;
      }

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

      // Check representation mismatch
      DCHECK_GT(predecessors.size(), 0);
      ValueRepresentation first_pred_repr =
          predecessors[0]->value_representation();
      bool representation_mismatch = false;
      for (int i = 1; i < predecessor_count; i++) {
        if (first_pred_repr != predecessors[i]->value_representation()) {
          representation_mismatch = true;
          break;
        }
      }

      if (representation_mismatch) {
        // This can happen due to type confusion on dead branches where we
        // perform type-unsafe stores (e.g. storing a Float64 to a field that
        // is tracked as Tagged on another path).
        TRACE("Representation mismatch for key " << key.data().offset << " of "
                                                 << PRINT_NODE(key.data().base)
                                                 << " -> escaping base");
        data_.MarkAsEscaped(key.data().base);
        need_revisit = true;
        return nullptr;
      }

      // Trying to find an already-created Phi for this field. If we find it, we
      // reuse it, for 2 reasons:
      //
      //   - performance (always): we avoid reallocating a new phi.
      //
      //   - correctness / termination (when {is_loop_fixpoint_check} is true):
      //     if there is already a Phi for this field, we reuse it and do not
      //     trigger a revisit of the loop. Creating a brand new phi would
      //     trigger loop revisits forever (since it always sets {need_revisit}
      //     to true).
      //
      // Note that when {is_loop_fixpoint_check} is false (for example, when an
      // outer loop revisits an inner loop), the forward-edge predecessors of
      // the inner loop header may have changed across outer-loop iterations.
      // Therefore, we update any changed inputs on the reused Phi to match the
      // incoming predecessors.
      for (auto [phi, other_key] : old_phis_) {
        if (other_key != key) continue;
        DCHECK_EQ(phi->input_count(), predecessors.size());
        bool any_input_changed = false;
        for (int i = 0; i < phi->input_count(); i++) {
          if (phi->input_node(i) != predecessors[i]) {
            DCHECK(!is_loop_fixpoint_check);
            phi->change_input(i, predecessors[i]);
            any_input_changed = true;
          }
        }
        DCHECK_IMPLIES(is_loop_fixpoint_check, !any_input_changed);
        RegisterNewPhi(phi, block, key);
        if (any_input_changed) {
          CandidateAnalyzer::ProcessPhi(phi);
        }
        return phi;
      }

      DCHECK(state.has_value());
      // The "owner" field of Phis is just used for exception phis late in the
      // Maglev backend (register allocator / code generator), which new Phis
      // inserted here will never be used for (since this phase is only used for
      // Turbolev). As such, we can use anything for the owner, and thus use
      // `Register::invalid_value`.
      constexpr interpreter::Register kFakeOwner =
          interpreter::Register::invalid_value();

      // When visiting a loop with multiple forward edge for the 1st time, we
      // may need to insert a phi to merge the forward values but we won't have
      // a backedge value yet. Still, we'll create a valid loop phi with enough
      // inputs and we'll set itself as backedge input.
      int phi_input_count =
          block->is_loop() ? block->predecessor_count() : predecessor_count;
      // TODO(dmercadier): instead of creating a proper Phi (which are 64 bytes
      // long + inputs!), we could have a custom "PseudoPhi" (name tbd)
      // structure that contains the bare minimum and would basically just be a
      // vector of Union(ValueNodes, PseudoPhi)., and only create real Phis in
      // the elider once we're sure that we're going to need them.
      Phi* phi = NodeBase::New<Phi>(zone(), phi_input_count, state.value(),
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
      if (block->is_loop() && predecessor_count < phi_input_count) {
        DCHECK_EQ(predecessor_count, phi_input_count - 1);
        phi->set_input(phi_input_count - 1, predecessors[0]);
      }
      phi->change_representation(predecessors[0]->value_representation());
      TRACE(">> Created new phi: " << PRINT_NODE(phi));
      need_revisit = true;
      RegisterNewPhi(phi, block, key);

      // We need to call `ProcessPhi` because an input of this phi could be an
      // eliding candidate, in which case we need to invalidate it.
      CandidateAnalyzer::ProcessPhi(phi);

      return phi;
    };

    field_values().StartNewSnapshot(base::VectorOf(predecessors_snapshots),
                                    merge_field_values);

    return need_revisit;
  }

  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    DCHECK(!field_values().IsSealed());
    Snapshot snapshot = field_values().Seal();
    block_snapshots_[block] = MaybeSnapshot{snapshot};

    if (JumpLoop* jump_loop = block->control_node()->TryCast<JumpLoop>()) {
      BasicBlock* loop_header = jump_loop->target();

      // Any candidate flowing into a backedge need to be invalidated. When this
      // happens, MarkAsEscaped will be called, which might set
      // has_escaped_candidate for the current loop, which could trigger a
      // revisit of the current loop. This means that it's important to run
      // MarkEscapingLoopPhiBackedges before popping the current loop from
      // `data_.loop_stack`.
      MarkEscapingLoopPhiBackedges(loop_header);

      // Loop phis backedges need to be patched in 2 situations:
      //
      //   - this is a loop with multiple forward edges that was requiring Phis
      //     to merge forward values. In that case, during the first visit of
      //     the loop, this Phi was created with itself as backedge (because it
      //     needs a backedge value to be a valid loop phi); and we're now
      //     patching this with the correct value of the backedge.
      //
      //   - we have just revisited the loop, and when creating the loop phis
      //     initially we were using the old backedge value (since it's the only
      //     one that we had); and we're now patching it with the correct value.
      //
      // Note that the fact that a loop phi needs to be patched isn't a reason
      // to revisit the loop: while visiting the loop, phis are treated as
      // opaque and we don't make any decisions that depend on the values of
      // the inputs of a Phi. So, changing the backedge input of loop phis
      // will not lead to making any different decision when revisiting the
      // loop.
      //
      // Similarly to MarkEscapingLoopPhiBackedges above, PatchLoopPhisBackedges
      // needs to be called before popping the current loop from
      // `data_.loop_stack` to ensure that if `MarkAsEscaped(alloc)` is called
      // inside it, it will find the current loop on the stack if needed.
      PatchLoopPhisBackedges(loop_header, snapshot);

      DCHECK_GT(data_.loop_stack.size(), 1);
      DCHECK_EQ(data_.loop_stack.back().header, loop_header);
      bool has_escaped_candidate =
          data_.loop_stack.back().has_escaped_candidate;
      data_.loop_stack.pop_back();

      auto prev_header_phis = new_phis().find(loop_header);
      if (prev_header_phis != new_phis().end()) {
        old_phis_ = std::move(*prev_header_phis->second);
        prev_header_phis->second->clear();
      }

      // TODO(dmercadier): we could try to reuse the snapshot created by
      // CreateSnapshotFor when revisiting the loop, instead of discarding it
      // and recomputing it afterwards.
      bool needs_revisit = has_escaped_candidate ||
                           CreateSnapshotFor(loop_header, true);
      if (needs_revisit) {
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
      InlinedAllocation* alloc = key.data().base;
      if (data_.HasEscaped(alloc)) {
        // If {alloc} was marked as escaping while visiting the loop, there is
        // no need to patch its backedge, in particular since calling
        // `fields_values().Get(key)` could produce a garbage value.
        continue;
      }

      ValueNode* backedge_val = field_values().Get(key);

      if (InlinedAllocation* backedge_alloc =
              data_.TryGetCandidateInlinedAllocation(backedge_val)) {
        // The backedge is an allocation that flows into a newly created loop
        // phi (which will be turned into a regular phi during the Elider
        // phase). We need to mark it as escaping. Note however that this
        // doesn't need to trigger a revisit of the loop: within the loop, 1)
        // any decisions make on the fact that {backedge_alloc} can be elided
        // still holds since it only escapes after the loop and 2) we never make
        // decisions based on the inputs of Phis, so the fact that the backedge
        // of {phi} is now escaping doesn't invalidate any decision made in the
        // loop.
        data_.MarkAsEscaped(backedge_alloc);
      }

      DCHECK_NOT_NULL(backedge_val);
      TRACE(">> Updating loop phi backedge: "
            << PRINT_NODE(phi) << " backedge "
            << PRINT_NODE(phi->input(backedge_index).node()) << " -> "
            << PRINT_NODE(backedge_val));
      phi->change_input(backedge_index, backedge_val);
    }

    field_values().Seal();
  }

  // Invalidates any candidate that flows into a loop phi. This might trigger a
  // loop revisit via has_escaped_candidate if the allocation was defined
  // outside the current loop.
  // TODO(dmercadier): try to merge objects in that case, instead of
  // marking them as escaping.
  void MarkEscapingLoopPhiBackedges(BasicBlock* loop_header) {
    TRACE("MarkEscapingLoopPhiBackedges");
    if (!loop_header->has_phi()) {
      TRACE("> no phis");
      return;
    }

    for (Phi* phi : *loop_header->phis()) {
      if (InlinedAllocation* alloc =
              data_.TryGetCandidateInlinedAllocation(phi->backedge_input())) {
        TRACE("> Marking " << NODE_ID(alloc) << " as escaping");
        data_.MarkAsEscaped(alloc);
      }
    }
  }

  ProcessResult Process(InlinedAllocation* node, const ProcessingState& state) {
    BasicBlock* current_loop = data_.loop_stack.back().header;
    data_.alloc_definition_loop[node] = current_loop;
    return CandidateAnalyzer::Process(node, state);
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

  // Stores the Phi nodes created at the loop header during the previous
  // iteration of the loop analysis.
  //
  // When merging the loop entry and backedge values on a revisit, we look up
  // the keys in this vector to reuse the existing Phis (updating their backedge
  // inputs in-place if they changed) rather than allocating new ones.
  // This is required to:
  //   1) Prevent duplicating Phi nodes in the Zone on every revisit.
  //   2) Ensure the loop analysis converges (otherwise, creating new Phis on
  //   every merge would trigger infinite loop revisits).
  //
  // This must be cleared before analyzing non-loop merges to prevent loop Phis
  // from being incorrectly reused inside the loop body.
  ZoneVector<std::pair<Phi*, Key>> old_phis_;

  Tracer tracer_;
};

void EscapeAnalysis::AnalyzeCandidates() {
  TRACE("EscapeAnalysis::AnalyzeCandidates");

  // Running a very quick pre-pass over the graph to spot candidates that
  // definitely escape and that thus don't need to be tracked. This makes the
  // main analysis faster and in some cases we can even just not run the main
  // analysis at all because the pre-pass will figure out that all candidates
  // actually escape.
  TRACE("Running the prepass...");
  // TODO(dmercadier): we should run the PrePass with the last processor before
  // Escape Analysis to save time. To do so, it would be nice if the PrePass
  // didn't need the EscapeAnalysisData. We can achieve this by not relying on
  // `data_.candidates` but instead use the HasEscaped bit of InlinedAllocation.
  GraphProcessor<AnalyzerPrePass> prepass(data_);
  prepass.ProcessGraph(data_.graph);

  if (!HasElidableCandidates()) {
    TRACE("> No candidates after the pre-pass; stopping here.");
    return;
  }

  TRACE("Running the main analysis...");
  GraphProcessor<FieldValuesTracker> main_analysis(data_);
  main_analysis.ProcessGraph(data_.graph);

  // Sanity check: {loop_stack} should be empty (except for its dummy initial
  // input), since for each loop we should have pushed once at the beginning and
  // popped once at the end.
  DCHECK_EQ(data_.loop_stack.size(), 1);
  DCHECK_EQ(data_.loop_stack.back().header, nullptr);

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
  if (data_.has_exception_handler) return false;
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
    DCHECK_IMPLIES(node->properties().has_eager_deopt_info(),
                   NodeT::kProperties.has_eager_deopt_info());
    DCHECK_IMPLIES(node->properties().can_lazy_deopt(),
                   NodeT::kProperties.can_lazy_deopt());

    // Updating deopt state.
    // TODO(dmercadier): we currently clone all DeoptInfo without even checking
    // if they have elided object inputs that would warrant cloning them. We
    // should instead first inspect the DeoptInfo to see if it has any input
    // that needs to be updated, and only then clone it.
    if constexpr (NodeT::kProperties.has_eager_deopt_info()) {
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
    static_assert(NodeT::kProperties.has_eager_deopt_info());
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

      if (replacement == nullptr || (replacement->value_representation() !=
                                     node->value_representation())) {
        // We have to be in unreachable code. Replacing by a DeadValue node
        // instead to avoid mismatches in the graph.
        // Note that this may sound a  bit risky: the mismatch could be because
        // of a bug in the escape analysis. However, even if this is the case,
        // inserting a DeadValue will lead to a reliable crash during compiling
        // (it it reaches the Turbolev graph builder) or at runtime (if it
        // reaches the MaglevGraphOptimizer, which will replace it by an
        // Unreachable).
        node->OverwriteWith(
            Opcode::kDeadValue,
            OpProperties::ForValueRepresentation(node->value_representation()));
        return ProcessResult::kContinue;
      }

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

  ProcessResult Process(LoadTaggedField* node, const ProcessingState& state) {
    return ProcessLoad(node, node->ValueInput().node(), node->offset());
  }

  ProcessResult Process(LoadFloat64* node, const ProcessingState& state) {
    return ProcessLoad(node, node->ValueInput().node(), node->offset());
  }

  ProcessResult Process(LoadFixedDoubleArrayElement* node,
                        const ProcessingState& state) {
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

// Static assertion check for a single node type.
// Note that It's not required for correctness that the AnalyzerPrePass has the
// same Process overloads as the CandidateAnalyzer, but keeping them in sync
// makes sure that the pre-pass doesn't wrongly treat an operation as escaping
// even though it's not.
// On the other hand, keeping the CandidateAnalyzer and Elider is required for
// correctness; cf comment at the beginning of this anonymous namespace.
#define CHECK_SYNCHRONIZED_OVERLOAD(NodeT)                                    \
  static_assert(HasSpecificProcess<CandidateAnalyzer, NodeT>() ==             \
                    HasSpecificProcess<AnalyzerPrePass, NodeT>(),             \
                "CandidateAnalyzer and Elider must be synchronized: both "    \
                "must either overload or not overload Process(" #NodeT "*)"); \
  static_assert(HasSpecificProcess<CandidateAnalyzer, NodeT>() ==             \
                    HasSpecificProcess<Elider, NodeT>(),                      \
                "CandidateAnalyzer and Elider must be synchronized: both "    \
                "must either overload or not overload Process(" #NodeT "*)");

// Run the check for all Maglev node types automatically
NODE_BASE_LIST(CHECK_SYNCHRONIZED_OVERLOAD)
#undef CHECK_SYNCHRONIZED_OVERLOAD

}  // namespace

}  // namespace v8::internal::maglev
