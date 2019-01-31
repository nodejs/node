// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/escape-analysis-reducer.h"

#include "src/compiler/all-nodes.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/frame-constants.h"

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

EscapeAnalysisReducer::EscapeAnalysisReducer(
    Editor* editor, JSGraph* jsgraph, EscapeAnalysisResult analysis_result,
    Zone* zone)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      analysis_result_(analysis_result),
      object_id_cache_(zone),
      node_cache_(jsgraph->graph(), zone),
      arguments_elements_(zone),
      zone_(zone) {}

Reduction EscapeAnalysisReducer::ReplaceNode(Node* original,
                                             Node* replacement) {
  const VirtualObject* vobject =
      analysis_result().GetVirtualObject(replacement);
  if (replacement->opcode() == IrOpcode::kDead ||
      (vobject && !vobject->HasEscaped())) {
    RelaxEffectsAndControls(original);
    return Replace(replacement);
  }
  Type const replacement_type = NodeProperties::GetType(replacement);
  Type const original_type = NodeProperties::GetType(original);
  if (replacement_type.Is(original_type)) {
    RelaxEffectsAndControls(original);
    return Replace(replacement);
  }

  // We need to guard the replacement if we would widen the type otherwise.
  DCHECK_EQ(1, original->op()->EffectOutputCount());
  DCHECK_EQ(1, original->op()->EffectInputCount());
  DCHECK_EQ(1, original->op()->ControlInputCount());
  Node* effect = NodeProperties::GetEffectInput(original);
  Node* control = NodeProperties::GetControlInput(original);
  original->TrimInputCount(0);
  original->AppendInput(jsgraph()->zone(), replacement);
  original->AppendInput(jsgraph()->zone(), effect);
  original->AppendInput(jsgraph()->zone(), control);
  NodeProperties::SetType(
      original,
      Type::Intersect(original_type, replacement_type, jsgraph()->zone()));
  NodeProperties::ChangeOp(original,
                           jsgraph()->common()->TypeGuard(original_type));
  ReplaceWithValue(original, original, original, control);
  return NoChange();
}

namespace {

Node* SkipTypeGuards(Node* node) {
  while (node->opcode() == IrOpcode::kTypeGuard) {
    node = NodeProperties::GetValueInput(node, 0);
  }
  return node;
}

}  // namespace

Node* EscapeAnalysisReducer::ObjectIdNode(const VirtualObject* vobject) {
  VirtualObject::Id id = vobject->id();
  if (id >= object_id_cache_.size()) object_id_cache_.resize(id + 1);
  if (!object_id_cache_[id]) {
    Node* node = jsgraph()->graph()->NewNode(jsgraph()->common()->ObjectId(id));
    NodeProperties::SetType(node, Type::Object());
    object_id_cache_[id] = node;
  }
  return object_id_cache_[id];
}

Reduction EscapeAnalysisReducer::Reduce(Node* node) {
  if (Node* replacement = analysis_result().GetReplacementOf(node)) {
    DCHECK(node->opcode() != IrOpcode::kAllocate &&
           node->opcode() != IrOpcode::kFinishRegion);
    DCHECK_NE(replacement, node);
    return ReplaceNode(node, replacement);
  }

  switch (node->opcode()) {
    case IrOpcode::kAllocate:
    case IrOpcode::kTypeGuard: {
      const VirtualObject* vobject = analysis_result().GetVirtualObject(node);
      if (vobject && !vobject->HasEscaped()) {
        RelaxEffectsAndControls(node);
      }
      return NoChange();
    }
    case IrOpcode::kFinishRegion: {
      Node* effect = NodeProperties::GetEffectInput(node, 0);
      if (effect->opcode() == IrOpcode::kBeginRegion) {
        RelaxEffectsAndControls(effect);
        RelaxEffectsAndControls(node);
      }
      return NoChange();
    }
    case IrOpcode::kNewArgumentsElements:
      arguments_elements_.insert(node);
      return NoChange();
    default: {
      // TODO(sigurds): Change this to GetFrameStateInputCount once
      // it is working. For now we use EffectInputCount > 0 to determine
      // whether a node might have a frame state input.
      if (node->op()->EffectInputCount() > 0) {
        ReduceFrameStateInputs(node);
      }
      return NoChange();
    }
  }
}

// While doing DFS on the FrameState tree, we have to recognize duplicate
// occurrences of virtual objects.
class Deduplicator {
 public:
  explicit Deduplicator(Zone* zone) : is_duplicate_(zone) {}
  bool SeenBefore(const VirtualObject* vobject) {
    VirtualObject::Id id = vobject->id();
    if (id >= is_duplicate_.size()) {
      is_duplicate_.resize(id + 1);
    }
    bool is_duplicate = is_duplicate_[id];
    is_duplicate_[id] = true;
    return is_duplicate;
  }

 private:
  ZoneVector<bool> is_duplicate_;
};

void EscapeAnalysisReducer::ReduceFrameStateInputs(Node* node) {
  DCHECK_GE(node->op()->EffectInputCount(), 1);
  for (int i = 0; i < node->InputCount(); ++i) {
    Node* input = node->InputAt(i);
    if (input->opcode() == IrOpcode::kFrameState) {
      Deduplicator deduplicator(zone());
      if (Node* ret = ReduceDeoptState(input, node, &deduplicator)) {
        node->ReplaceInput(i, ret);
      }
    }
  }
}

Node* EscapeAnalysisReducer::ReduceDeoptState(Node* node, Node* effect,
                                              Deduplicator* deduplicator) {
  if (node->opcode() == IrOpcode::kFrameState) {
    NodeHashCache::Constructor new_node(&node_cache_, node);
    // This input order is important to match the DFS traversal used in the
    // instruction selector. Otherwise, the instruction selector might find a
    // duplicate node before the original one.
    for (int input_id : {kFrameStateOuterStateInput, kFrameStateFunctionInput,
                         kFrameStateParametersInput, kFrameStateContextInput,
                         kFrameStateLocalsInput, kFrameStateStackInput}) {
      Node* input = node->InputAt(input_id);
      new_node.ReplaceInput(ReduceDeoptState(input, effect, deduplicator),
                            input_id);
    }
    return new_node.Get();
  } else if (node->opcode() == IrOpcode::kStateValues) {
    NodeHashCache::Constructor new_node(&node_cache_, node);
    for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
      Node* input = NodeProperties::GetValueInput(node, i);
      new_node.ReplaceValueInput(ReduceDeoptState(input, effect, deduplicator),
                                 i);
    }
    return new_node.Get();
  } else if (const VirtualObject* vobject =
                 analysis_result().GetVirtualObject(SkipTypeGuards(node))) {
    if (vobject->HasEscaped()) return node;
    if (deduplicator->SeenBefore(vobject)) {
      return ObjectIdNode(vobject);
    } else {
      std::vector<Node*> inputs;
      for (int offset = 0; offset < vobject->size(); offset += kTaggedSize) {
        Node* field =
            analysis_result().GetVirtualObjectField(vobject, offset, effect);
        CHECK_NOT_NULL(field);
        if (field != jsgraph()->Dead()) {
          inputs.push_back(ReduceDeoptState(field, effect, deduplicator));
        }
      }
      int num_inputs = static_cast<int>(inputs.size());
      NodeHashCache::Constructor new_node(
          &node_cache_,
          jsgraph()->common()->ObjectState(vobject->id(), num_inputs),
          num_inputs, &inputs.front(), NodeProperties::GetType(node));
      return new_node.Get();
    }
  } else {
    return node;
  }
}

void EscapeAnalysisReducer::VerifyReplacement() const {
  AllNodes all(zone(), jsgraph()->graph());
  for (Node* node : all.reachable) {
    if (node->opcode() == IrOpcode::kAllocate) {
      if (const VirtualObject* vobject =
              analysis_result().GetVirtualObject(node)) {
        if (!vobject->HasEscaped()) {
          FATAL("Escape analysis failed to remove node %s#%d\n",
                node->op()->mnemonic(), node->id());
        }
      }
    }
  }
}

void EscapeAnalysisReducer::Finalize() {
  for (Node* node : arguments_elements_) {
    int mapped_count = NewArgumentsElementsMappedCountOf(node->op());

    Node* arguments_frame = NodeProperties::GetValueInput(node, 0);
    if (arguments_frame->opcode() != IrOpcode::kArgumentsFrame) continue;
    Node* arguments_length = NodeProperties::GetValueInput(node, 1);
    if (arguments_length->opcode() != IrOpcode::kArgumentsLength) continue;

    // If mapped arguments are specified, then their number is always equal to
    // the number of formal parameters. This allows to use just the three-value
    // {ArgumentsStateType} enum because the deoptimizer can reconstruct the
    // value of {mapped_count} from the number of formal parameters.
    DCHECK_IMPLIES(
        mapped_count != 0,
        mapped_count == FormalParameterCountOf(arguments_length->op()));
    ArgumentsStateType type = IsRestLengthOf(arguments_length->op())
                                  ? ArgumentsStateType::kRestParameter
                                  : (mapped_count == 0)
                                        ? ArgumentsStateType::kUnmappedArguments
                                        : ArgumentsStateType::kMappedArguments;

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
                jsgraph()->common()->ArgumentsLengthState(type));
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
          if (mapped_count == 0) {
            loads.push_back(use);
          } else {
            escaping_use = true;
          }
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
          jsgraph()->common()->ArgumentsElementsState(type));
      NodeProperties::SetType(arguments_elements_state, Type::OtherInternal());
      ReplaceWithValue(node, arguments_elements_state);

      ElementAccess stack_access;
      stack_access.base_is_tagged = BaseTaggedness::kUntaggedBase;
      // Reduce base address by {kSystemPointerSize} such that (length - index)
      // resolves to the right position.
      stack_access.header_size =
          CommonFrameConstants::kFixedFrameSizeAboveFp - kSystemPointerSize;
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
                                    TypeCache::Get()->kArgumentsLengthType);
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

Node* NodeHashCache::Query(Node* node) {
  auto it = cache_.find(node);
  if (it != cache_.end()) {
    return *it;
  } else {
    return nullptr;
  }
}

NodeHashCache::Constructor::Constructor(NodeHashCache* cache,
                                        const Operator* op, int input_count,
                                        Node** inputs, Type type)
    : node_cache_(cache), from_(nullptr) {
  if (node_cache_->temp_nodes_.size() > 0) {
    tmp_ = node_cache_->temp_nodes_.back();
    node_cache_->temp_nodes_.pop_back();
    int tmp_input_count = tmp_->InputCount();
    if (input_count <= tmp_input_count) {
      tmp_->TrimInputCount(input_count);
    }
    for (int i = 0; i < input_count; ++i) {
      if (i < tmp_input_count) {
        tmp_->ReplaceInput(i, inputs[i]);
      } else {
        tmp_->AppendInput(node_cache_->graph_->zone(), inputs[i]);
      }
    }
    NodeProperties::ChangeOp(tmp_, op);
  } else {
    tmp_ = node_cache_->graph_->NewNode(op, input_count, inputs);
  }
  NodeProperties::SetType(tmp_, type);
}

Node* NodeHashCache::Constructor::Get() {
  DCHECK(tmp_ || from_);
  Node* node;
  if (!tmp_) {
    node = node_cache_->Query(from_);
    if (!node) node = from_;
  } else {
    node = node_cache_->Query(tmp_);
    if (node) {
      node_cache_->temp_nodes_.push_back(tmp_);
    } else {
      node = tmp_;
      node_cache_->Insert(node);
    }
  }
  tmp_ = from_ = nullptr;
  return node;
}

Node* NodeHashCache::Constructor::MutableNode() {
  DCHECK(tmp_ || from_);
  if (!tmp_) {
    if (node_cache_->temp_nodes_.empty()) {
      tmp_ = node_cache_->graph_->CloneNode(from_);
    } else {
      tmp_ = node_cache_->temp_nodes_.back();
      node_cache_->temp_nodes_.pop_back();
      int from_input_count = from_->InputCount();
      int tmp_input_count = tmp_->InputCount();
      if (from_input_count <= tmp_input_count) {
        tmp_->TrimInputCount(from_input_count);
      }
      for (int i = 0; i < from_input_count; ++i) {
        if (i < tmp_input_count) {
          tmp_->ReplaceInput(i, from_->InputAt(i));
        } else {
          tmp_->AppendInput(node_cache_->graph_->zone(), from_->InputAt(i));
        }
      }
      NodeProperties::SetType(tmp_, NodeProperties::GetType(from_));
      NodeProperties::ChangeOp(tmp_, from_->op());
    }
  }
  return tmp_;
}

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
