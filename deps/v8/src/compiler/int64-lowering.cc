// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/int64-lowering.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/diamond.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/wasm-compiler.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

Int64Lowering::Int64Lowering(Graph* graph, MachineOperatorBuilder* machine,
                             CommonOperatorBuilder* common, Zone* zone,
                             Signature<MachineRepresentation>* signature)
    : zone_(zone),
      graph_(graph),
      machine_(machine),
      common_(common),
      state_(graph, 3),
      stack_(zone),
      replacements_(nullptr),
      signature_(signature),
      placeholder_(graph->NewNode(common->Parameter(-2, "placeholder"),
                                  graph->start())) {
  DCHECK_NOT_NULL(graph);
  DCHECK_NOT_NULL(graph->end());
  replacements_ = zone->NewArray<Replacement>(graph->NodeCount());
  memset(replacements_, 0, sizeof(Replacement) * graph->NodeCount());
}

void Int64Lowering::LowerGraph() {
  if (!machine()->Is32()) {
    return;
  }
  stack_.push_back({graph()->end(), 0});
  state_.Set(graph()->end(), State::kOnStack);

  while (!stack_.empty()) {
    NodeState& top = stack_.back();
    if (top.input_index == top.node->InputCount()) {
      // All inputs of top have already been lowered, now lower top.
      stack_.pop_back();
      state_.Set(top.node, State::kVisited);
      LowerNode(top.node);
    } else {
      // Push the next input onto the stack.
      Node* input = top.node->InputAt(top.input_index++);
      if (state_.Get(input) == State::kUnvisited) {
        if (input->opcode() == IrOpcode::kPhi) {
          // To break cycles with phi nodes we push phis on a separate stack so
          // that they are processed after all other nodes.
          PreparePhiReplacement(input);
          stack_.push_front({input, 0});
        } else if (input->opcode() == IrOpcode::kEffectPhi ||
                   input->opcode() == IrOpcode::kLoop) {
          stack_.push_front({input, 0});
        } else {
          stack_.push_back({input, 0});
        }
        state_.Set(input, State::kOnStack);
      }
    }
  }
}

namespace {

int GetReturnIndexAfterLowering(
    CallDescriptor* descriptor, int old_index) {
  int result = old_index;
  for (int i = 0; i < old_index; i++) {
    if (descriptor->GetReturnType(i).representation() ==
        MachineRepresentation::kWord64) {
      result++;
    }
  }
  return result;
}

int GetReturnCountAfterLowering(CallDescriptor* descriptor) {
  return GetReturnIndexAfterLowering(
      descriptor, static_cast<int>(descriptor->ReturnCount()));
}

int GetParameterIndexAfterLowering(
    Signature<MachineRepresentation>* signature, int old_index) {
  int result = old_index;
  for (int i = 0; i < old_index; i++) {
    if (signature->GetParam(i) == MachineRepresentation::kWord64) {
      result++;
    }
  }
  return result;
}

int GetReturnCountAfterLowering(Signature<MachineRepresentation>* signature) {
  int result = static_cast<int>(signature->return_count());
  for (int i = 0; i < static_cast<int>(signature->return_count()); i++) {
    if (signature->GetReturn(i) == MachineRepresentation::kWord64) {
      result++;
    }
  }
  return result;
}

}  // namespace

// static
int Int64Lowering::GetParameterCountAfterLowering(
    Signature<MachineRepresentation>* signature) {
  // GetParameterIndexAfterLowering(parameter_count) returns the parameter count
  // after lowering.
  return GetParameterIndexAfterLowering(
      signature, static_cast<int>(signature->parameter_count()));
}

// static
bool Int64Lowering::IsI64AsTwoParameters(MachineOperatorBuilder* machine,
                                         MachineRepresentation type) {
  return machine->Is32() && type == MachineRepresentation::kWord64;
}

void Int64Lowering::GetIndexNodes(Node* index, Node*& index_low,
                                  Node*& index_high) {
  if (HasReplacementLow(index)) {
    index = GetReplacementLow(index);
  }
#if defined(V8_TARGET_LITTLE_ENDIAN)
  index_low = index;
  index_high = graph()->NewNode(machine()->Int32Add(), index,
                                graph()->NewNode(common()->Int32Constant(4)));
#elif defined(V8_TARGET_BIG_ENDIAN)
  index_low = graph()->NewNode(machine()->Int32Add(), index,
                               graph()->NewNode(common()->Int32Constant(4)));
  index_high = index;
#endif
}

void Int64Lowering::LowerNode(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kInt64Constant: {
      int64_t value = OpParameter<int64_t>(node);
      Node* low_node = graph()->NewNode(
          common()->Int32Constant(static_cast<int32_t>(value & 0xFFFFFFFF)));
      Node* high_node = graph()->NewNode(
          common()->Int32Constant(static_cast<int32_t>(value >> 32)));
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kLoad:
    case IrOpcode::kUnalignedLoad: {
      MachineRepresentation rep;
      if (node->opcode() == IrOpcode::kLoad) {
        rep = LoadRepresentationOf(node->op()).representation();
      } else {
        DCHECK_EQ(IrOpcode::kUnalignedLoad, node->opcode());
        rep = UnalignedLoadRepresentationOf(node->op()).representation();
      }

      if (rep == MachineRepresentation::kWord64) {
        Node* base = node->InputAt(0);
        Node* index = node->InputAt(1);
        Node* index_low;
        Node* index_high;
        GetIndexNodes(index, index_low, index_high);
        const Operator* load_op;

        if (node->opcode() == IrOpcode::kLoad) {
          load_op = machine()->Load(MachineType::Int32());
        } else {
          DCHECK_EQ(IrOpcode::kUnalignedLoad, node->opcode());
          load_op = machine()->UnalignedLoad(MachineType::Int32());
        }

        Node* high_node;
        if (node->InputCount() > 2) {
          Node* effect_high = node->InputAt(2);
          Node* control_high = node->InputAt(3);
          high_node = graph()->NewNode(load_op, base, index_high, effect_high,
                                       control_high);
          // change the effect change from old_node --> old_effect to
          // old_node --> high_node --> old_effect.
          node->ReplaceInput(2, high_node);
        } else {
          high_node = graph()->NewNode(load_op, base, index_high);
        }
        node->ReplaceInput(1, index_low);
        NodeProperties::ChangeOp(node, load_op);
        ReplaceNode(node, node, high_node);
      } else {
        DefaultLowering(node);
      }
      break;
    }
    case IrOpcode::kStore:
    case IrOpcode::kUnalignedStore: {
      MachineRepresentation rep;
      if (node->opcode() == IrOpcode::kStore) {
        rep = StoreRepresentationOf(node->op()).representation();
      } else {
        DCHECK_EQ(IrOpcode::kUnalignedStore, node->opcode());
        rep = UnalignedStoreRepresentationOf(node->op());
      }

      if (rep == MachineRepresentation::kWord64) {
        // We change the original store node to store the low word, and create
        // a new store node to store the high word. The effect and control edges
        // are copied from the original store to the new store node, the effect
        // edge of the original store is redirected to the new store.
        Node* base = node->InputAt(0);
        Node* index = node->InputAt(1);
        Node* index_low;
        Node* index_high;
        GetIndexNodes(index, index_low, index_high);
        Node* value = node->InputAt(2);
        DCHECK(HasReplacementLow(value));
        DCHECK(HasReplacementHigh(value));

        const Operator* store_op;
        if (node->opcode() == IrOpcode::kStore) {
          WriteBarrierKind write_barrier_kind =
              StoreRepresentationOf(node->op()).write_barrier_kind();
          store_op = machine()->Store(StoreRepresentation(
              MachineRepresentation::kWord32, write_barrier_kind));
        } else {
          DCHECK_EQ(IrOpcode::kUnalignedStore, node->opcode());
          store_op = machine()->UnalignedStore(MachineRepresentation::kWord32);
        }

        Node* high_node;
        if (node->InputCount() > 3) {
          Node* effect_high = node->InputAt(3);
          Node* control_high = node->InputAt(4);
          high_node = graph()->NewNode(store_op, base, index_high,
                                       GetReplacementHigh(value), effect_high,
                                       control_high);
          node->ReplaceInput(3, high_node);

        } else {
          high_node = graph()->NewNode(store_op, base, index_high,
                                       GetReplacementHigh(value));
        }

        node->ReplaceInput(1, index_low);
        node->ReplaceInput(2, GetReplacementLow(value));
        NodeProperties::ChangeOp(node, store_op);
        ReplaceNode(node, node, high_node);
      } else {
        DefaultLowering(node, true);
      }
      break;
    }
    case IrOpcode::kStart: {
      int parameter_count = GetParameterCountAfterLowering(signature());
      // Only exchange the node if the parameter count actually changed.
      if (parameter_count != static_cast<int>(signature()->parameter_count())) {
        int delta =
            parameter_count - static_cast<int>(signature()->parameter_count());
        int new_output_count = node->op()->ValueOutputCount() + delta;
        NodeProperties::ChangeOp(node, common()->Start(new_output_count));
      }
      break;
    }
    case IrOpcode::kParameter: {
      DCHECK_EQ(1, node->InputCount());
      // Only exchange the node if the parameter count actually changed. We do
      // not even have to do the default lowering because the the start node,
      // the only input of a parameter node, only changes if the parameter count
      // changes.
      if (GetParameterCountAfterLowering(signature()) !=
          static_cast<int>(signature()->parameter_count())) {
        int old_index = ParameterIndexOf(node->op());
        // TODO(wasm): Make this part not wasm specific.
        // Prevent special lowering of the WasmContext parameter.
        if (old_index == kWasmContextParameterIndex) {
          DefaultLowering(node);
          break;
        }
        // Adjust old_index to be compliant with the signature.
        --old_index;
        int new_index = GetParameterIndexAfterLowering(signature(), old_index);
        // Adjust new_index to consider the WasmContext parameter.
        ++new_index;
        NodeProperties::ChangeOp(node, common()->Parameter(new_index));

        if (signature()->GetParam(old_index) ==
            MachineRepresentation::kWord64) {
          Node* high_node = graph()->NewNode(common()->Parameter(new_index + 1),
                                             graph()->start());
          ReplaceNode(node, node, high_node);
        }
      }
      break;
    }
    case IrOpcode::kReturn: {
      int input_count = node->InputCount();
      DefaultLowering(node);
      if (input_count != node->InputCount()) {
        int new_return_count = GetReturnCountAfterLowering(signature());
        if (static_cast<int>(signature()->return_count()) != new_return_count) {
          NodeProperties::ChangeOp(node, common()->Return(new_return_count));
        }
      }
      break;
    }
    case IrOpcode::kTailCall: {
      CallDescriptor* descriptor =
          const_cast<CallDescriptor*>(CallDescriptorOf(node->op()));
      bool returns_require_lowering =
          GetReturnCountAfterLowering(descriptor) !=
          static_cast<int>(descriptor->ReturnCount());
      if (DefaultLowering(node) || returns_require_lowering) {
        // Tail calls do not have return values, so adjusting the call
        // descriptor is enough.
        auto new_descriptor = GetI32WasmCallDescriptor(zone(), descriptor);
        NodeProperties::ChangeOp(node, common()->TailCall(new_descriptor));
      }
      break;
    }
    case IrOpcode::kCall: {
      CallDescriptor* descriptor =
          const_cast<CallDescriptor*>(CallDescriptorOf(node->op()));
      bool returns_require_lowering =
          GetReturnCountAfterLowering(descriptor) !=
              static_cast<int>(descriptor->ReturnCount());
      if (DefaultLowering(node) || returns_require_lowering) {
        // We have to adjust the call descriptor.
        NodeProperties::ChangeOp(
            node, common()->Call(GetI32WasmCallDescriptor(zone(), descriptor)));
      }
      if (returns_require_lowering) {
        size_t return_arity = descriptor->ReturnCount();
        if (return_arity == 1) {
          // We access the additional return values through projections.
          Node* low_node =
              graph()->NewNode(common()->Projection(0), node, graph()->start());
          Node* high_node =
              graph()->NewNode(common()->Projection(1), node, graph()->start());
          ReplaceNode(node, low_node, high_node);
        } else {
          ZoneVector<Node*> projections(return_arity, zone());
          NodeProperties::CollectValueProjections(node, projections.data(),
                                                  return_arity);
          for (size_t old_index = 0, new_index = 0; old_index < return_arity;
               ++old_index, ++new_index) {
            Node* use_node = projections[old_index];
            DCHECK_EQ(ProjectionIndexOf(use_node->op()), old_index);
            DCHECK_EQ(GetReturnIndexAfterLowering(descriptor,
                                                  static_cast<int>(old_index)),
                      static_cast<int>(new_index));
            if (new_index != old_index) {
              NodeProperties::ChangeOp(
                  use_node, common()->Projection(new_index));
            }
            if (descriptor->GetReturnType(old_index).representation() ==
                MachineRepresentation::kWord64) {
              Node* high_node = graph()->NewNode(
                  common()->Projection(new_index + 1), node,
                  graph()->start());
              ReplaceNode(use_node, use_node, high_node);
              ++new_index;
            }
          }
        }
      }
      break;
    }
    case IrOpcode::kWord64And: {
      DCHECK_EQ(2, node->InputCount());
      Node* left = node->InputAt(0);
      Node* right = node->InputAt(1);

      Node* low_node =
          graph()->NewNode(machine()->Word32And(), GetReplacementLow(left),
                           GetReplacementLow(right));
      Node* high_node =
          graph()->NewNode(machine()->Word32And(), GetReplacementHigh(left),
                           GetReplacementHigh(right));
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kTruncateInt64ToInt32: {
      DCHECK_EQ(1, node->InputCount());
      Node* input = node->InputAt(0);
      ReplaceNode(node, GetReplacementLow(input), nullptr);
      node->NullAllInputs();
      break;
    }
    case IrOpcode::kInt64Add: {
      DCHECK_EQ(2, node->InputCount());

      Node* right = node->InputAt(1);
      node->ReplaceInput(1, GetReplacementLow(right));
      node->AppendInput(zone(), GetReplacementHigh(right));

      Node* left = node->InputAt(0);
      node->ReplaceInput(0, GetReplacementLow(left));
      node->InsertInput(zone(), 1, GetReplacementHigh(left));

      NodeProperties::ChangeOp(node, machine()->Int32PairAdd());
      // We access the additional return values through projections.
      Node* low_node =
          graph()->NewNode(common()->Projection(0), node, graph()->start());
      Node* high_node =
          graph()->NewNode(common()->Projection(1), node, graph()->start());
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kInt64Sub: {
      DCHECK_EQ(2, node->InputCount());

      Node* right = node->InputAt(1);
      node->ReplaceInput(1, GetReplacementLow(right));
      node->AppendInput(zone(), GetReplacementHigh(right));

      Node* left = node->InputAt(0);
      node->ReplaceInput(0, GetReplacementLow(left));
      node->InsertInput(zone(), 1, GetReplacementHigh(left));

      NodeProperties::ChangeOp(node, machine()->Int32PairSub());
      // We access the additional return values through projections.
      Node* low_node =
          graph()->NewNode(common()->Projection(0), node, graph()->start());
      Node* high_node =
          graph()->NewNode(common()->Projection(1), node, graph()->start());
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kInt64Mul: {
      DCHECK_EQ(2, node->InputCount());

      Node* right = node->InputAt(1);
      node->ReplaceInput(1, GetReplacementLow(right));
      node->AppendInput(zone(), GetReplacementHigh(right));

      Node* left = node->InputAt(0);
      node->ReplaceInput(0, GetReplacementLow(left));
      node->InsertInput(zone(), 1, GetReplacementHigh(left));

      NodeProperties::ChangeOp(node, machine()->Int32PairMul());
      // We access the additional return values through projections.
      Node* low_node =
          graph()->NewNode(common()->Projection(0), node, graph()->start());
      Node* high_node =
          graph()->NewNode(common()->Projection(1), node, graph()->start());
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kWord64Or: {
      DCHECK_EQ(2, node->InputCount());
      Node* left = node->InputAt(0);
      Node* right = node->InputAt(1);

      Node* low_node =
          graph()->NewNode(machine()->Word32Or(), GetReplacementLow(left),
                           GetReplacementLow(right));
      Node* high_node =
          graph()->NewNode(machine()->Word32Or(), GetReplacementHigh(left),
                           GetReplacementHigh(right));
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kWord64Xor: {
      DCHECK_EQ(2, node->InputCount());
      Node* left = node->InputAt(0);
      Node* right = node->InputAt(1);

      Node* low_node =
          graph()->NewNode(machine()->Word32Xor(), GetReplacementLow(left),
                           GetReplacementLow(right));
      Node* high_node =
          graph()->NewNode(machine()->Word32Xor(), GetReplacementHigh(left),
                           GetReplacementHigh(right));
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kWord64Shl: {
      // TODO(turbofan): if the shift count >= 32, then we can set the low word
      // of the output to 0 and just calculate the high word.
      DCHECK_EQ(2, node->InputCount());
      Node* shift = node->InputAt(1);
      if (HasReplacementLow(shift)) {
        // We do not have to care about the high word replacement, because
        // the shift can only be between 0 and 63 anyways.
        node->ReplaceInput(1, GetReplacementLow(shift));
      }

      Node* value = node->InputAt(0);
      node->ReplaceInput(0, GetReplacementLow(value));
      node->InsertInput(zone(), 1, GetReplacementHigh(value));

      NodeProperties::ChangeOp(node, machine()->Word32PairShl());
      // We access the additional return values through projections.
      Node* low_node =
          graph()->NewNode(common()->Projection(0), node, graph()->start());
      Node* high_node =
          graph()->NewNode(common()->Projection(1), node, graph()->start());
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kWord64Shr: {
      // TODO(turbofan): if the shift count >= 32, then we can set the low word
      // of the output to 0 and just calculate the high word.
      DCHECK_EQ(2, node->InputCount());
      Node* shift = node->InputAt(1);
      if (HasReplacementLow(shift)) {
        // We do not have to care about the high word replacement, because
        // the shift can only be between 0 and 63 anyways.
        node->ReplaceInput(1, GetReplacementLow(shift));
      }

      Node* value = node->InputAt(0);
      node->ReplaceInput(0, GetReplacementLow(value));
      node->InsertInput(zone(), 1, GetReplacementHigh(value));

      NodeProperties::ChangeOp(node, machine()->Word32PairShr());
      // We access the additional return values through projections.
      Node* low_node =
          graph()->NewNode(common()->Projection(0), node, graph()->start());
      Node* high_node =
          graph()->NewNode(common()->Projection(1), node, graph()->start());
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kWord64Sar: {
      // TODO(turbofan): if the shift count >= 32, then we can set the low word
      // of the output to 0 and just calculate the high word.
      DCHECK_EQ(2, node->InputCount());
      Node* shift = node->InputAt(1);
      if (HasReplacementLow(shift)) {
        // We do not have to care about the high word replacement, because
        // the shift can only be between 0 and 63 anyways.
        node->ReplaceInput(1, GetReplacementLow(shift));
      }

      Node* value = node->InputAt(0);
      node->ReplaceInput(0, GetReplacementLow(value));
      node->InsertInput(zone(), 1, GetReplacementHigh(value));

      NodeProperties::ChangeOp(node, machine()->Word32PairSar());
      // We access the additional return values through projections.
      Node* low_node =
          graph()->NewNode(common()->Projection(0), node, graph()->start());
      Node* high_node =
          graph()->NewNode(common()->Projection(1), node, graph()->start());
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kWord64Equal: {
      DCHECK_EQ(2, node->InputCount());
      Node* left = node->InputAt(0);
      Node* right = node->InputAt(1);

      // TODO(wasm): Use explicit comparisons and && here?
      Node* replacement = graph()->NewNode(
          machine()->Word32Equal(),
          graph()->NewNode(
              machine()->Word32Or(),
              graph()->NewNode(machine()->Word32Xor(), GetReplacementLow(left),
                               GetReplacementLow(right)),
              graph()->NewNode(machine()->Word32Xor(), GetReplacementHigh(left),
                               GetReplacementHigh(right))),
          graph()->NewNode(common()->Int32Constant(0)));

      ReplaceNode(node, replacement, nullptr);
      break;
    }
    case IrOpcode::kInt64LessThan: {
      LowerComparison(node, machine()->Int32LessThan(),
                      machine()->Uint32LessThan());
      break;
    }
    case IrOpcode::kInt64LessThanOrEqual: {
      LowerComparison(node, machine()->Int32LessThan(),
                      machine()->Uint32LessThanOrEqual());
      break;
    }
    case IrOpcode::kUint64LessThan: {
      LowerComparison(node, machine()->Uint32LessThan(),
                      machine()->Uint32LessThan());
      break;
    }
    case IrOpcode::kUint64LessThanOrEqual: {
      LowerComparison(node, machine()->Uint32LessThan(),
                      machine()->Uint32LessThanOrEqual());
      break;
    }
    case IrOpcode::kChangeInt32ToInt64: {
      DCHECK_EQ(1, node->InputCount());
      Node* input = node->InputAt(0);
      if (HasReplacementLow(input)) {
        input = GetReplacementLow(input);
      }
      // We use SAR to preserve the sign in the high word.
      ReplaceNode(
          node, input,
          graph()->NewNode(machine()->Word32Sar(), input,
                           graph()->NewNode(common()->Int32Constant(31))));
      node->NullAllInputs();
      break;
    }
    case IrOpcode::kChangeUint32ToUint64: {
      DCHECK_EQ(1, node->InputCount());
      Node* input = node->InputAt(0);
      if (HasReplacementLow(input)) {
        input = GetReplacementLow(input);
      }
      ReplaceNode(node, input, graph()->NewNode(common()->Int32Constant(0)));
      node->NullAllInputs();
      break;
    }
    case IrOpcode::kBitcastInt64ToFloat64: {
      DCHECK_EQ(1, node->InputCount());
      Node* input = node->InputAt(0);
      Node* stack_slot = graph()->NewNode(
          machine()->StackSlot(MachineRepresentation::kWord64));

      Node* store_high_word = graph()->NewNode(
          machine()->Store(
              StoreRepresentation(MachineRepresentation::kWord32,
                                  WriteBarrierKind::kNoWriteBarrier)),
          stack_slot,
          graph()->NewNode(
              common()->Int32Constant(kInt64UpperHalfMemoryOffset)),
          GetReplacementHigh(input), graph()->start(), graph()->start());

      Node* store_low_word = graph()->NewNode(
          machine()->Store(
              StoreRepresentation(MachineRepresentation::kWord32,
                                  WriteBarrierKind::kNoWriteBarrier)),
          stack_slot,
          graph()->NewNode(
              common()->Int32Constant(kInt64LowerHalfMemoryOffset)),
          GetReplacementLow(input), store_high_word, graph()->start());

      Node* load =
          graph()->NewNode(machine()->Load(MachineType::Float64()), stack_slot,
                           graph()->NewNode(common()->Int32Constant(0)),
                           store_low_word, graph()->start());

      ReplaceNode(node, load, nullptr);
      break;
    }
    case IrOpcode::kBitcastFloat64ToInt64: {
      DCHECK_EQ(1, node->InputCount());
      Node* input = node->InputAt(0);
      if (HasReplacementLow(input)) {
        input = GetReplacementLow(input);
      }
      Node* stack_slot = graph()->NewNode(
          machine()->StackSlot(MachineRepresentation::kWord64));
      Node* store = graph()->NewNode(
          machine()->Store(
              StoreRepresentation(MachineRepresentation::kFloat64,
                                  WriteBarrierKind::kNoWriteBarrier)),
          stack_slot, graph()->NewNode(common()->Int32Constant(0)), input,
          graph()->start(), graph()->start());

      Node* high_node = graph()->NewNode(
          machine()->Load(MachineType::Int32()), stack_slot,
          graph()->NewNode(
              common()->Int32Constant(kInt64UpperHalfMemoryOffset)),
          store, graph()->start());

      Node* low_node = graph()->NewNode(
          machine()->Load(MachineType::Int32()), stack_slot,
          graph()->NewNode(
              common()->Int32Constant(kInt64LowerHalfMemoryOffset)),
          store, graph()->start());
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kWord64Ror: {
      DCHECK_EQ(2, node->InputCount());
      Node* input = node->InputAt(0);
      Node* shift = HasReplacementLow(node->InputAt(1))
                        ? GetReplacementLow(node->InputAt(1))
                        : node->InputAt(1);
      Int32Matcher m(shift);
      if (m.HasValue()) {
        // Precondition: 0 <= shift < 64.
        int32_t shift_value = m.Value() & 0x3F;
        if (shift_value == 0) {
          ReplaceNode(node, GetReplacementLow(input),
                      GetReplacementHigh(input));
        } else if (shift_value == 32) {
          ReplaceNode(node, GetReplacementHigh(input),
                      GetReplacementLow(input));
        } else {
          Node* low_input;
          Node* high_input;
          if (shift_value < 32) {
            low_input = GetReplacementLow(input);
            high_input = GetReplacementHigh(input);
          } else {
            low_input = GetReplacementHigh(input);
            high_input = GetReplacementLow(input);
          }
          int32_t masked_shift_value = shift_value & 0x1F;
          Node* masked_shift =
              graph()->NewNode(common()->Int32Constant(masked_shift_value));
          Node* inv_shift = graph()->NewNode(
              common()->Int32Constant(32 - masked_shift_value));

          Node* low_node = graph()->NewNode(
              machine()->Word32Or(),
              graph()->NewNode(machine()->Word32Shr(), low_input, masked_shift),
              graph()->NewNode(machine()->Word32Shl(), high_input, inv_shift));
          Node* high_node = graph()->NewNode(
              machine()->Word32Or(), graph()->NewNode(machine()->Word32Shr(),
                                                      high_input, masked_shift),
              graph()->NewNode(machine()->Word32Shl(), low_input, inv_shift));
          ReplaceNode(node, low_node, high_node);
        }
      } else {
        Node* safe_shift = shift;
        if (!machine()->Word32ShiftIsSafe()) {
          safe_shift =
              graph()->NewNode(machine()->Word32And(), shift,
                               graph()->NewNode(common()->Int32Constant(0x1F)));
        }

        // By creating this bit-mask with SAR and SHL we do not have to deal
        // with shift == 0 as a special case.
        Node* inv_mask = graph()->NewNode(
            machine()->Word32Shl(),
            graph()->NewNode(machine()->Word32Sar(),
                             graph()->NewNode(common()->Int32Constant(
                                 std::numeric_limits<int32_t>::min())),
                             safe_shift),
            graph()->NewNode(common()->Int32Constant(1)));

        Node* bit_mask =
            graph()->NewNode(machine()->Word32Xor(), inv_mask,
                             graph()->NewNode(common()->Int32Constant(-1)));

        // We have to mask the shift value for this comparison. If
        // !machine()->Word32ShiftIsSafe() then the masking should already be
        // part of the graph.
        Node* masked_shift6 = shift;
        if (machine()->Word32ShiftIsSafe()) {
          masked_shift6 =
              graph()->NewNode(machine()->Word32And(), shift,
                               graph()->NewNode(common()->Int32Constant(0x3F)));
        }

        Diamond lt32(
            graph(), common(),
            graph()->NewNode(machine()->Int32LessThan(), masked_shift6,
                             graph()->NewNode(common()->Int32Constant(32))));

        // The low word and the high word can be swapped either at the input or
        // at the output. We swap the inputs so that shift does not have to be
        // kept for so long in a register.
        Node* input_low =
            lt32.Phi(MachineRepresentation::kWord32, GetReplacementLow(input),
                     GetReplacementHigh(input));
        Node* input_high =
            lt32.Phi(MachineRepresentation::kWord32, GetReplacementHigh(input),
                     GetReplacementLow(input));

        Node* rotate_low =
            graph()->NewNode(machine()->Word32Ror(), input_low, safe_shift);
        Node* rotate_high =
            graph()->NewNode(machine()->Word32Ror(), input_high, safe_shift);

        Node* low_node = graph()->NewNode(
            machine()->Word32Or(),
            graph()->NewNode(machine()->Word32And(), rotate_low, bit_mask),
            graph()->NewNode(machine()->Word32And(), rotate_high, inv_mask));

        Node* high_node = graph()->NewNode(
            machine()->Word32Or(),
            graph()->NewNode(machine()->Word32And(), rotate_high, bit_mask),
            graph()->NewNode(machine()->Word32And(), rotate_low, inv_mask));

        ReplaceNode(node, low_node, high_node);
      }
      break;
    }
    case IrOpcode::kWord64Clz: {
      DCHECK_EQ(1, node->InputCount());
      Node* input = node->InputAt(0);
      Diamond d(
          graph(), common(),
          graph()->NewNode(machine()->Word32Equal(), GetReplacementHigh(input),
                           graph()->NewNode(common()->Int32Constant(0))));

      Node* low_node = d.Phi(
          MachineRepresentation::kWord32,
          graph()->NewNode(machine()->Int32Add(),
                           graph()->NewNode(machine()->Word32Clz(),
                                            GetReplacementLow(input)),
                           graph()->NewNode(common()->Int32Constant(32))),
          graph()->NewNode(machine()->Word32Clz(), GetReplacementHigh(input)));
      ReplaceNode(node, low_node, graph()->NewNode(common()->Int32Constant(0)));
      break;
    }
    case IrOpcode::kWord64Ctz: {
      DCHECK_EQ(1, node->InputCount());
      DCHECK(machine()->Word32Ctz().IsSupported());
      Node* input = node->InputAt(0);
      Diamond d(
          graph(), common(),
          graph()->NewNode(machine()->Word32Equal(), GetReplacementLow(input),
                           graph()->NewNode(common()->Int32Constant(0))));
      Node* low_node =
          d.Phi(MachineRepresentation::kWord32,
                graph()->NewNode(machine()->Int32Add(),
                                 graph()->NewNode(machine()->Word32Ctz().op(),
                                                  GetReplacementHigh(input)),
                                 graph()->NewNode(common()->Int32Constant(32))),
                graph()->NewNode(machine()->Word32Ctz().op(),
                                 GetReplacementLow(input)));
      ReplaceNode(node, low_node, graph()->NewNode(common()->Int32Constant(0)));
      break;
    }
    case IrOpcode::kWord64Popcnt: {
      DCHECK_EQ(1, node->InputCount());
      Node* input = node->InputAt(0);
      // We assume that a Word64Popcnt node only has been created if
      // Word32Popcnt is actually supported.
      DCHECK(machine()->Word32Popcnt().IsSupported());
      ReplaceNode(node, graph()->NewNode(
                            machine()->Int32Add(),
                            graph()->NewNode(machine()->Word32Popcnt().op(),
                                             GetReplacementLow(input)),
                            graph()->NewNode(machine()->Word32Popcnt().op(),
                                             GetReplacementHigh(input))),
                  graph()->NewNode(common()->Int32Constant(0)));
      break;
    }
    case IrOpcode::kPhi: {
      MachineRepresentation rep = PhiRepresentationOf(node->op());
      if (rep == MachineRepresentation::kWord64) {
        // The replacement nodes have already been created, we only have to
        // replace placeholder nodes.
        Node* low_node = GetReplacementLow(node);
        Node* high_node = GetReplacementHigh(node);
        for (int i = 0; i < node->op()->ValueInputCount(); i++) {
          low_node->ReplaceInput(i, GetReplacementLow(node->InputAt(i)));
          high_node->ReplaceInput(i, GetReplacementHigh(node->InputAt(i)));
        }
      } else {
        DefaultLowering(node);
      }
      break;
    }
    case IrOpcode::kWord64ReverseBytes: {
      Node* input = node->InputAt(0);
      ReplaceNode(node, graph()->NewNode(machine()->Word32ReverseBytes().op(),
                                         GetReplacementHigh(input)),
                  graph()->NewNode(machine()->Word32ReverseBytes().op(),
                                   GetReplacementLow(input)));
      break;
    }

    default: { DefaultLowering(node); }
  }
}  // NOLINT(readability/fn_size)

void Int64Lowering::LowerComparison(Node* node, const Operator* high_word_op,
                                    const Operator* low_word_op) {
  DCHECK_EQ(2, node->InputCount());
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Node* replacement = graph()->NewNode(
      machine()->Word32Or(),
      graph()->NewNode(high_word_op, GetReplacementHigh(left),
                       GetReplacementHigh(right)),
      graph()->NewNode(
          machine()->Word32And(),
          graph()->NewNode(machine()->Word32Equal(), GetReplacementHigh(left),
                           GetReplacementHigh(right)),
          graph()->NewNode(low_word_op, GetReplacementLow(left),
                           GetReplacementLow(right))));

  ReplaceNode(node, replacement, nullptr);
}

bool Int64Lowering::DefaultLowering(Node* node, bool low_word_only) {
  bool something_changed = false;
  for (int i = NodeProperties::PastValueIndex(node) - 1; i >= 0; i--) {
    Node* input = node->InputAt(i);
    if (HasReplacementLow(input)) {
      something_changed = true;
      node->ReplaceInput(i, GetReplacementLow(input));
    }
    if (!low_word_only && HasReplacementHigh(input)) {
      something_changed = true;
      node->InsertInput(zone(), i + 1, GetReplacementHigh(input));
    }
  }
  return something_changed;
}

void Int64Lowering::ReplaceNode(Node* old, Node* new_low, Node* new_high) {
  // if new_low == nullptr, then also new_high == nullptr.
  DCHECK(new_low != nullptr || new_high == nullptr);
  replacements_[old->id()].low = new_low;
  replacements_[old->id()].high = new_high;
}

bool Int64Lowering::HasReplacementLow(Node* node) {
  return replacements_[node->id()].low != nullptr;
}

Node* Int64Lowering::GetReplacementLow(Node* node) {
  Node* result = replacements_[node->id()].low;
  DCHECK(result);
  return result;
}

bool Int64Lowering::HasReplacementHigh(Node* node) {
  return replacements_[node->id()].high != nullptr;
}

Node* Int64Lowering::GetReplacementHigh(Node* node) {
  Node* result = replacements_[node->id()].high;
  DCHECK(result);
  return result;
}

void Int64Lowering::PreparePhiReplacement(Node* phi) {
  MachineRepresentation rep = PhiRepresentationOf(phi->op());
  if (rep == MachineRepresentation::kWord64) {
    // We have to create the replacements for a phi node before we actually
    // lower the phi to break potential cycles in the graph. The replacements of
    // input nodes do not exist yet, so we use a placeholder node to pass the
    // graph verifier.
    int value_count = phi->op()->ValueInputCount();
    Node** inputs_low = zone()->NewArray<Node*>(value_count + 1);
    Node** inputs_high = zone()->NewArray<Node*>(value_count + 1);
    for (int i = 0; i < value_count; i++) {
      inputs_low[i] = placeholder_;
      inputs_high[i] = placeholder_;
    }
    inputs_low[value_count] = NodeProperties::GetControlInput(phi, 0);
    inputs_high[value_count] = NodeProperties::GetControlInput(phi, 0);
    ReplaceNode(phi,
                graph()->NewNode(
                    common()->Phi(MachineRepresentation::kWord32, value_count),
                    value_count + 1, inputs_low, false),
                graph()->NewNode(
                    common()->Phi(MachineRepresentation::kWord32, value_count),
                    value_count + 1, inputs_high, false));
  }
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
