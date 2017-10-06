// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/escape-analysis-reducer.h"

#include "src/compiler/all-nodes.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/counters.h"

namespace v8 {
namespace internal {
namespace compiler {

#ifdef DEBUG
#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_turbo_escape) PrintF(__VA_ARGS__); \
  } while (false)
#else
#define TRACE(...)
#endif  // DEBUG

EscapeAnalysisReducer::EscapeAnalysisReducer(Editor* editor, JSGraph* jsgraph,
                                             EscapeAnalysis* escape_analysis,
                                             Zone* zone)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      escape_analysis_(escape_analysis),
      zone_(zone),
      fully_reduced_(static_cast<int>(jsgraph->graph()->NodeCount() * 2), zone),
      exists_virtual_allocate_(escape_analysis->ExistsVirtualAllocate()) {}

Reduction EscapeAnalysisReducer::ReduceNode(Node* node) {
  if (node->id() < static_cast<NodeId>(fully_reduced_.length()) &&
      fully_reduced_.Contains(node->id())) {
    return NoChange();
  }

  switch (node->opcode()) {
    case IrOpcode::kLoadField:
    case IrOpcode::kLoadElement:
      return ReduceLoad(node);
    case IrOpcode::kStoreField:
    case IrOpcode::kStoreElement:
      return ReduceStore(node);
    case IrOpcode::kCheckMaps:
      return ReduceCheckMaps(node);
    case IrOpcode::kAllocate:
      return ReduceAllocate(node);
    case IrOpcode::kFinishRegion:
      return ReduceFinishRegion(node);
    case IrOpcode::kReferenceEqual:
      return ReduceReferenceEqual(node);
    case IrOpcode::kObjectIsSmi:
      return ReduceObjectIsSmi(node);
    // FrameStates and Value nodes are preprocessed here,
    // and visited via ReduceFrameStateUses from their user nodes.
    case IrOpcode::kFrameState:
    case IrOpcode::kStateValues: {
      if (node->id() >= static_cast<NodeId>(fully_reduced_.length()) ||
          fully_reduced_.Contains(node->id())) {
        break;
      }
      bool depends_on_object_state = false;
      for (Node* input : node->inputs()) {
        switch (input->opcode()) {
          case IrOpcode::kAllocate:
          case IrOpcode::kFinishRegion:
            depends_on_object_state =
                depends_on_object_state || escape_analysis()->IsVirtual(input);
            break;
          case IrOpcode::kFrameState:
          case IrOpcode::kStateValues:
            depends_on_object_state =
                depends_on_object_state ||
                input->id() >= static_cast<NodeId>(fully_reduced_.length()) ||
                !fully_reduced_.Contains(input->id());
            break;
          default:
            break;
        }
      }
      if (!depends_on_object_state) {
        fully_reduced_.Add(node->id());
      }
      return NoChange();
    }
    case IrOpcode::kNewUnmappedArgumentsElements:
      arguments_elements_.insert(node);
      break;
    default:
      // TODO(sigurds): Change this to GetFrameStateInputCount once
      // it is working. For now we use EffectInputCount > 0 to determine
      // whether a node might have a frame state input.
      if (exists_virtual_allocate_ && node->op()->EffectInputCount() > 0) {
        return ReduceFrameStateUses(node);
      }
      break;
  }
  return NoChange();
}

Reduction EscapeAnalysisReducer::Reduce(Node* node) {
  Reduction reduction = ReduceNode(node);
  if (reduction.Changed() && node != reduction.replacement()) {
    escape_analysis()->SetReplacement(node, reduction.replacement());
  }
  return reduction;
}

namespace {

Node* MaybeGuard(JSGraph* jsgraph, Zone* zone, Node* original,
                 Node* replacement) {
  // We might need to guard the replacement if the type of the {replacement}
  // node is not in a sub-type relation to the type of the the {original} node.
  Type* const replacement_type = NodeProperties::GetType(replacement);
  Type* const original_type = NodeProperties::GetType(original);
  if (!replacement_type->Is(original_type)) {
    Node* const control = NodeProperties::GetControlInput(original);
    replacement = jsgraph->graph()->NewNode(
        jsgraph->common()->TypeGuard(original_type), replacement, control);
    NodeProperties::SetType(replacement, original_type);
  }
  return replacement;
}

Node* SkipTypeGuards(Node* node) {
  while (node->opcode() == IrOpcode::kTypeGuard) {
    node = NodeProperties::GetValueInput(node, 0);
  }
  return node;
}

}  // namespace

Reduction EscapeAnalysisReducer::ReduceLoad(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kLoadField ||
         node->opcode() == IrOpcode::kLoadElement);
  if (node->id() < static_cast<NodeId>(fully_reduced_.length())) {
    fully_reduced_.Add(node->id());
  }
  if (escape_analysis()->IsVirtual(
          SkipTypeGuards(NodeProperties::GetValueInput(node, 0)))) {
    if (Node* rep = escape_analysis()->GetReplacement(node)) {
      TRACE("Replaced #%d (%s) with #%d (%s)\n", node->id(),
            node->op()->mnemonic(), rep->id(), rep->op()->mnemonic());
      rep = MaybeGuard(jsgraph(), zone(), node, rep);
      ReplaceWithValue(node, rep);
      return Replace(rep);
    }
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceStore(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kStoreField ||
         node->opcode() == IrOpcode::kStoreElement);
  if (node->id() < static_cast<NodeId>(fully_reduced_.length())) {
    fully_reduced_.Add(node->id());
  }
  if (escape_analysis()->IsVirtual(
          SkipTypeGuards(NodeProperties::GetValueInput(node, 0)))) {
    TRACE("Removed #%d (%s) from effect chain\n", node->id(),
          node->op()->mnemonic());
    RelaxEffectsAndControls(node);
    return Changed(node);
  }
  return NoChange();
}

Reduction EscapeAnalysisReducer::ReduceCheckMaps(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kCheckMaps);
  if (node->id() < static_cast<NodeId>(fully_reduced_.length())) {
    fully_reduced_.Add(node->id());
  }
  if (escape_analysis()->IsVirtual(
          SkipTypeGuards(NodeProperties::GetValueInput(node, 0))) &&
      !escape_analysis()->IsEscaped(node)) {
    TRACE("Removed #%d (%s) from effect chain\n", node->id(),
          node->op()->mnemonic());
    RelaxEffectsAndControls(node);
    return Changed(node);
  }
  return NoChange();
}

Reduction EscapeAnalysisReducer::ReduceAllocate(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAllocate);
  if (node->id() < static_cast<NodeId>(fully_reduced_.length())) {
    fully_reduced_.Add(node->id());
  }
  if (escape_analysis()->IsVirtual(node)) {
    RelaxEffectsAndControls(node);
    TRACE("Removed allocate #%d from effect chain\n", node->id());
    return Changed(node);
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceFinishRegion(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kFinishRegion);
  Node* effect = NodeProperties::GetEffectInput(node, 0);
  if (effect->opcode() == IrOpcode::kBeginRegion) {
    // We only add it now to remove empty Begin/Finish region pairs
    // in the process.
    if (node->id() < static_cast<NodeId>(fully_reduced_.length())) {
      fully_reduced_.Add(node->id());
    }
    RelaxEffectsAndControls(effect);
    RelaxEffectsAndControls(node);
#ifdef DEBUG
    if (FLAG_trace_turbo_escape) {
      PrintF("Removed region #%d / #%d from effect chain,", effect->id(),
             node->id());
      PrintF(" %d user(s) of #%d remain(s):", node->UseCount(), node->id());
      for (Edge edge : node->use_edges()) {
        PrintF(" #%d", edge.from()->id());
      }
      PrintF("\n");
    }
#endif  // DEBUG
    return Changed(node);
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceReferenceEqual(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kReferenceEqual);
  Node* left = SkipTypeGuards(NodeProperties::GetValueInput(node, 0));
  Node* right = SkipTypeGuards(NodeProperties::GetValueInput(node, 1));
  if (escape_analysis()->IsVirtual(left)) {
    if (escape_analysis()->IsVirtual(right) &&
        escape_analysis()->CompareVirtualObjects(left, right)) {
      ReplaceWithValue(node, jsgraph()->TrueConstant());
      TRACE("Replaced ref eq #%d with true\n", node->id());
      return Replace(jsgraph()->TrueConstant());
    }
    // Right-hand side is not a virtual object, or a different one.
    ReplaceWithValue(node, jsgraph()->FalseConstant());
    TRACE("Replaced ref eq #%d with false\n", node->id());
    return Replace(jsgraph()->FalseConstant());
  } else if (escape_analysis()->IsVirtual(right)) {
    // Left-hand side is not a virtual object.
    ReplaceWithValue(node, jsgraph()->FalseConstant());
    TRACE("Replaced ref eq #%d with false\n", node->id());
    return Replace(jsgraph()->FalseConstant());
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceObjectIsSmi(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kObjectIsSmi);
  Node* input = SkipTypeGuards(NodeProperties::GetValueInput(node, 0));
  if (escape_analysis()->IsVirtual(input)) {
    ReplaceWithValue(node, jsgraph()->FalseConstant());
    TRACE("Replaced ObjectIsSmi #%d with false\n", node->id());
    return Replace(jsgraph()->FalseConstant());
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceFrameStateUses(Node* node) {
  DCHECK_GE(node->op()->EffectInputCount(), 1);
  if (node->id() < static_cast<NodeId>(fully_reduced_.length())) {
    fully_reduced_.Add(node->id());
  }
  bool changed = false;
  for (int i = 0; i < node->InputCount(); ++i) {
    Node* input = node->InputAt(i);
    if (input->opcode() == IrOpcode::kFrameState) {
      if (Node* ret = ReduceDeoptState(input, node, false)) {
        node->ReplaceInput(i, ret);
        changed = true;
      }
    }
  }
  if (changed) {
    return Changed(node);
  }
  return NoChange();
}


// Returns the clone if it duplicated the node, and null otherwise.
Node* EscapeAnalysisReducer::ReduceDeoptState(Node* node, Node* effect,
                                              bool multiple_users) {
  DCHECK(node->opcode() == IrOpcode::kFrameState ||
         node->opcode() == IrOpcode::kStateValues);
  if (node->id() < static_cast<NodeId>(fully_reduced_.length()) &&
      fully_reduced_.Contains(node->id())) {
    return nullptr;
  }
  TRACE("Reducing %s %d\n", node->op()->mnemonic(), node->id());
  Node* clone = nullptr;
  bool node_multiused = node->UseCount() > 1;
  bool multiple_users_rec = multiple_users || node_multiused;
  for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
    Node* input = NodeProperties::GetValueInput(node, i);
    if (input->opcode() == IrOpcode::kStateValues) {
      if (Node* ret = ReduceDeoptState(input, effect, multiple_users_rec)) {
        if (node_multiused || (multiple_users && !clone)) {
          TRACE("  Cloning #%d", node->id());
          node = clone = jsgraph()->graph()->CloneNode(node);
          TRACE(" to #%d\n", node->id());
          node_multiused = false;
        }
        NodeProperties::ReplaceValueInput(node, ret, i);
      }
    } else {
      if (Node* ret = ReduceStateValueInput(node, i, effect, node_multiused,
                                            clone, multiple_users)) {
        DCHECK_NULL(clone);
        node_multiused = false;  // Don't clone anymore.
        node = clone = ret;
      }
    }
  }
  if (node->opcode() == IrOpcode::kFrameState) {
    Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
    if (outer_frame_state->opcode() == IrOpcode::kFrameState) {
      if (Node* ret =
              ReduceDeoptState(outer_frame_state, effect, multiple_users_rec)) {
        if (node_multiused || (multiple_users && !clone)) {
          TRACE("    Cloning #%d", node->id());
          node = clone = jsgraph()->graph()->CloneNode(node);
          TRACE(" to #%d\n", node->id());
        }
        NodeProperties::ReplaceFrameStateInput(node, ret);
      }
    }
  }
  if (node->id() < static_cast<NodeId>(fully_reduced_.length())) {
    fully_reduced_.Add(node->id());
  }
  return clone;
}


// Returns the clone if it duplicated the node, and null otherwise.
Node* EscapeAnalysisReducer::ReduceStateValueInput(Node* node, int node_index,
                                                   Node* effect,
                                                   bool node_multiused,
                                                   bool already_cloned,
                                                   bool multiple_users) {
  Node* input = SkipTypeGuards(NodeProperties::GetValueInput(node, node_index));
  if (node->id() < static_cast<NodeId>(fully_reduced_.length()) &&
      fully_reduced_.Contains(node->id())) {
    return nullptr;
  }
  TRACE("Reducing State Input #%d (%s)\n", input->id(),
        input->op()->mnemonic());
  Node* clone = nullptr;
  if (input->opcode() == IrOpcode::kFinishRegion ||
      input->opcode() == IrOpcode::kAllocate) {
    if (escape_analysis()->IsVirtual(input)) {
      if (escape_analysis()->IsCyclicObjectState(effect, input)) {
        // TODO(mstarzinger): Represent cyclic object states differently to
        // ensure the scheduler can properly handle such object states.
        compilation_failed_ = true;
        return nullptr;
      }
      if (Node* object_state =
              escape_analysis()->GetOrCreateObjectState(effect, input)) {
        if (node_multiused || (multiple_users && !already_cloned)) {
          TRACE("Cloning #%d", node->id());
          node = clone = jsgraph()->graph()->CloneNode(node);
          TRACE(" to #%d\n", node->id());
          node_multiused = false;
          already_cloned = true;
        }
        NodeProperties::ReplaceValueInput(node, object_state, node_index);
        TRACE("Replaced state #%d input #%d with object state #%d\n",
              node->id(), input->id(), object_state->id());
      } else {
        TRACE("No object state replacement for #%d at effect #%d available.\n",
              input->id(), effect->id());
        UNREACHABLE();
      }
    }
  }
  return clone;
}


void EscapeAnalysisReducer::VerifyReplacement() const {
#ifdef DEBUG
  AllNodes all(zone(), jsgraph()->graph());
  for (Node* node : all.reachable) {
    if (node->opcode() == IrOpcode::kAllocate) {
      CHECK(!escape_analysis_->IsVirtual(node));
    }
  }
#endif  // DEBUG
}

void EscapeAnalysisReducer::Finalize() {
  for (Node* node : arguments_elements_) {
    DCHECK(node->opcode() == IrOpcode::kNewUnmappedArgumentsElements);

    Node* arguments_frame = NodeProperties::GetValueInput(node, 0);
    if (arguments_frame->opcode() != IrOpcode::kArgumentsFrame) continue;
    Node* arguments_length = NodeProperties::GetValueInput(node, 1);
    if (arguments_length->opcode() != IrOpcode::kArgumentsLength) continue;

    Node* arguments_length_state = nullptr;
    for (Edge edge : arguments_length->use_edges()) {
      Node* use = edge.from();
      switch (use->opcode()) {
        case IrOpcode::kObjectState:
        case IrOpcode::kTypedObjectState:
        case IrOpcode::kStateValues:
        case IrOpcode::kTypedStateValues:
          if (!arguments_length_state) {
            arguments_length_state = jsgraph()->graph()->NewNode(
                jsgraph()->common()->ArgumentsLengthState(
                    IsRestLengthOf(arguments_length->op())));
            NodeProperties::SetType(arguments_length_state,
                                    Type::OtherInternal());
          }
          edge.UpdateTo(arguments_length_state);
          break;
        default:
          break;
      }
    }

    bool escaping_use = false;
    ZoneVector<Node*> loads(zone());
    for (Edge edge : node->use_edges()) {
      Node* use = edge.from();
      if (!NodeProperties::IsValueEdge(edge)) continue;
      if (use->use_edges().empty()) {
        // A node without uses is dead, so we don't have to care about it.
        continue;
      }
      switch (use->opcode()) {
        case IrOpcode::kStateValues:
        case IrOpcode::kTypedStateValues:
        case IrOpcode::kObjectState:
        case IrOpcode::kTypedObjectState:
          break;
        case IrOpcode::kLoadElement:
          loads.push_back(use);
          break;
        case IrOpcode::kLoadField:
          if (FieldAccessOf(use->op()).offset == FixedArray::kLengthOffset) {
            loads.push_back(use);
          } else {
            escaping_use = true;
          }
          break;
        default:
          // If the arguments elements node node is used by an unhandled node,
          // then we cannot remove this allocation.
          escaping_use = true;
          break;
      }
      if (escaping_use) break;
    }
    if (!escaping_use) {
      Node* arguments_elements_state = jsgraph()->graph()->NewNode(
          jsgraph()->common()->ArgumentsElementsState(
              IsRestLengthOf(arguments_length->op())));
      NodeProperties::SetType(arguments_elements_state, Type::OtherInternal());
      ReplaceWithValue(node, arguments_elements_state);

      ElementAccess stack_access;
      stack_access.base_is_tagged = BaseTaggedness::kUntaggedBase;
      // Reduce base address by {kPointerSize} such that (length - index)
      // resolves to the right position.
      stack_access.header_size =
          CommonFrameConstants::kFixedFrameSizeAboveFp - kPointerSize;
      stack_access.type = Type::NonInternal();
      stack_access.machine_type = MachineType::AnyTagged();
      stack_access.write_barrier_kind = WriteBarrierKind::kNoWriteBarrier;
      const Operator* load_stack_op =
          jsgraph()->simplified()->LoadElement(stack_access);

      for (Node* load : loads) {
        switch (load->opcode()) {
          case IrOpcode::kLoadElement: {
            Node* index = NodeProperties::GetValueInput(load, 1);
            // {offset} is a reverted index starting from 1. The base address is
            // adapted to allow offsets starting from 1.
            Node* offset = jsgraph()->graph()->NewNode(
                jsgraph()->simplified()->NumberSubtract(), arguments_length,
                index);
            NodeProperties::SetType(offset,
                                    TypeCache::Get().kArgumentsLengthType);
            NodeProperties::ReplaceValueInput(load, arguments_frame, 0);
            NodeProperties::ReplaceValueInput(load, offset, 1);
            NodeProperties::ChangeOp(load, load_stack_op);
            break;
          }
          case IrOpcode::kLoadField: {
            DCHECK_EQ(FieldAccessOf(load->op()).offset,
                      FixedArray::kLengthOffset);
            Node* length = NodeProperties::GetValueInput(node, 1);
            ReplaceWithValue(load, length);
            break;
          }
          default:
            UNREACHABLE();
        }
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
