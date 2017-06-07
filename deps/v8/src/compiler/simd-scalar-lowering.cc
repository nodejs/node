// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simd-scalar-lowering.h"
#include "src/compiler/diamond.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

#include "src/compiler/node.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace compiler {

SimdScalarLowering::SimdScalarLowering(
    JSGraph* jsgraph, Signature<MachineRepresentation>* signature)
    : jsgraph_(jsgraph),
      state_(jsgraph->graph(), 3),
      stack_(jsgraph_->zone()),
      replacements_(nullptr),
      signature_(signature),
      placeholder_(graph()->NewNode(common()->Parameter(-2, "placeholder"),
                                    graph()->start())),
      parameter_count_after_lowering_(-1) {
  DCHECK_NOT_NULL(graph());
  DCHECK_NOT_NULL(graph()->end());
  replacements_ = zone()->NewArray<Replacement>(graph()->NodeCount());
  memset(replacements_, 0, sizeof(Replacement) * graph()->NodeCount());
}

void SimdScalarLowering::LowerGraph() {
  stack_.push_back({graph()->end(), 0});
  state_.Set(graph()->end(), State::kOnStack);
  replacements_[graph()->end()->id()].type = SimdType::kInt32;

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
        SetLoweredType(input, top.node);
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

#define FOREACH_INT32X4_OPCODE(V) \
  V(I32x4Splat)                   \
  V(I32x4ExtractLane)             \
  V(I32x4ReplaceLane)             \
  V(I32x4SConvertF32x4)           \
  V(I32x4UConvertF32x4)           \
  V(I32x4Neg)                     \
  V(I32x4Add)                     \
  V(I32x4Sub)                     \
  V(I32x4Mul)                     \
  V(I32x4MinS)                    \
  V(I32x4MaxS)                    \
  V(I32x4MinU)                    \
  V(I32x4MaxU)                    \
  V(S128And)                      \
  V(S128Or)                       \
  V(S128Xor)                      \
  V(S128Not)

#define FOREACH_FLOAT32X4_OPCODE(V) \
  V(F32x4Splat)                     \
  V(F32x4ExtractLane)               \
  V(F32x4ReplaceLane)               \
  V(F32x4SConvertI32x4)             \
  V(F32x4UConvertI32x4)             \
  V(F32x4Abs)                       \
  V(F32x4Neg)                       \
  V(F32x4Add)                       \
  V(F32x4Sub)                       \
  V(F32x4Mul)                       \
  V(F32x4Div)                       \
  V(F32x4Min)                       \
  V(F32x4Max)

#define FOREACH_FLOAT32X4_TO_SIMD1X4OPCODE(V) \
  V(F32x4Eq)                                  \
  V(F32x4Ne)                                  \
  V(F32x4Lt)                                  \
  V(F32x4Le)                                  \
  V(F32x4Gt)                                  \
  V(F32x4Ge)

#define FOREACH_INT32X4_TO_SIMD1X4OPCODE(V) \
  V(I32x4Eq)                                \
  V(I32x4Ne)                                \
  V(I32x4LtS)                               \
  V(I32x4LeS)                               \
  V(I32x4GtS)                               \
  V(I32x4GeS)                               \
  V(I32x4LtU)                               \
  V(I32x4LeU)                               \
  V(I32x4GtU)                               \
  V(I32x4GeU)

void SimdScalarLowering::SetLoweredType(Node* node, Node* output) {
  switch (node->opcode()) {
#define CASE_STMT(name) case IrOpcode::k##name:
    FOREACH_INT32X4_OPCODE(CASE_STMT)
    case IrOpcode::kReturn:
    case IrOpcode::kParameter:
    case IrOpcode::kCall: {
      replacements_[node->id()].type = SimdType::kInt32;
      break;
    }
      FOREACH_FLOAT32X4_OPCODE(CASE_STMT) {
        replacements_[node->id()].type = SimdType::kFloat32;
        break;
      }
      FOREACH_FLOAT32X4_TO_SIMD1X4OPCODE(CASE_STMT)
      FOREACH_INT32X4_TO_SIMD1X4OPCODE(CASE_STMT) {
        replacements_[node->id()].type = SimdType::kSimd1x4;
        break;
      }
    default: {
      switch (output->opcode()) {
        FOREACH_FLOAT32X4_TO_SIMD1X4OPCODE(CASE_STMT)
        case IrOpcode::kF32x4SConvertI32x4:
        case IrOpcode::kF32x4UConvertI32x4: {
          replacements_[node->id()].type = SimdType::kInt32;
          break;
        }
          FOREACH_INT32X4_TO_SIMD1X4OPCODE(CASE_STMT)
        case IrOpcode::kI32x4SConvertF32x4:
        case IrOpcode::kI32x4UConvertF32x4: {
          replacements_[node->id()].type = SimdType::kFloat32;
          break;
        }
        case IrOpcode::kS32x4Select: {
          replacements_[node->id()].type = SimdType::kSimd1x4;
          break;
        }
        default: {
          replacements_[node->id()].type = replacements_[output->id()].type;
        }
      }
    }
#undef CASE_STMT
  }
}

static int GetParameterIndexAfterLowering(
    Signature<MachineRepresentation>* signature, int old_index) {
  // In function calls, the simd128 types are passed as 4 Int32 types. The
  // parameters are typecast to the types as needed for various operations.
  int result = old_index;
  for (int i = 0; i < old_index; ++i) {
    if (signature->GetParam(i) == MachineRepresentation::kSimd128) {
      result += 3;
    }
  }
  return result;
}

int SimdScalarLowering::GetParameterCountAfterLowering() {
  if (parameter_count_after_lowering_ == -1) {
    // GetParameterIndexAfterLowering(parameter_count) returns the parameter
    // count after lowering.
    parameter_count_after_lowering_ = GetParameterIndexAfterLowering(
        signature(), static_cast<int>(signature()->parameter_count()));
  }
  return parameter_count_after_lowering_;
}

static int GetReturnCountAfterLowering(
    Signature<MachineRepresentation>* signature) {
  int result = static_cast<int>(signature->return_count());
  for (int i = 0; i < static_cast<int>(signature->return_count()); ++i) {
    if (signature->GetReturn(i) == MachineRepresentation::kSimd128) {
      result += 3;
    }
  }
  return result;
}

void SimdScalarLowering::GetIndexNodes(Node* index, Node** new_indices) {
  new_indices[0] = index;
  for (size_t i = 1; i < kMaxLanes; ++i) {
    new_indices[i] = graph()->NewNode(machine()->Int32Add(), index,
                                      graph()->NewNode(common()->Int32Constant(
                                          static_cast<int>(i) * kLaneWidth)));
  }
}

void SimdScalarLowering::LowerLoadOp(MachineRepresentation rep, Node* node,
                                     const Operator* load_op) {
  if (rep == MachineRepresentation::kSimd128) {
    Node* base = node->InputAt(0);
    Node* index = node->InputAt(1);
    Node* indices[kMaxLanes];
    GetIndexNodes(index, indices);
    Node* rep_nodes[kMaxLanes];
    rep_nodes[0] = node;
    NodeProperties::ChangeOp(rep_nodes[0], load_op);
    if (node->InputCount() > 2) {
      DCHECK(node->InputCount() > 3);
      Node* effect_input = node->InputAt(2);
      Node* control_input = node->InputAt(3);
      rep_nodes[3] = graph()->NewNode(load_op, base, indices[3], effect_input,
                                      control_input);
      rep_nodes[2] = graph()->NewNode(load_op, base, indices[2], rep_nodes[3],
                                      control_input);
      rep_nodes[1] = graph()->NewNode(load_op, base, indices[1], rep_nodes[2],
                                      control_input);
      rep_nodes[0]->ReplaceInput(2, rep_nodes[1]);
    } else {
      for (size_t i = 1; i < kMaxLanes; ++i) {
        rep_nodes[i] = graph()->NewNode(load_op, base, indices[i]);
      }
    }
    ReplaceNode(node, rep_nodes);
  } else {
    DefaultLowering(node);
  }
}

void SimdScalarLowering::LowerStoreOp(MachineRepresentation rep, Node* node,
                                      const Operator* store_op,
                                      SimdType rep_type) {
  if (rep == MachineRepresentation::kSimd128) {
    Node* base = node->InputAt(0);
    Node* index = node->InputAt(1);
    Node* indices[kMaxLanes];
    GetIndexNodes(index, indices);
    DCHECK(node->InputCount() > 2);
    Node* value = node->InputAt(2);
    DCHECK(HasReplacement(1, value));
    Node* rep_nodes[kMaxLanes];
    rep_nodes[0] = node;
    Node** rep_inputs = GetReplacementsWithType(value, rep_type);
    rep_nodes[0]->ReplaceInput(2, rep_inputs[0]);
    NodeProperties::ChangeOp(node, store_op);
    if (node->InputCount() > 3) {
      DCHECK(node->InputCount() > 4);
      Node* effect_input = node->InputAt(3);
      Node* control_input = node->InputAt(4);
      rep_nodes[3] = graph()->NewNode(store_op, base, indices[3], rep_inputs[3],
                                      effect_input, control_input);
      rep_nodes[2] = graph()->NewNode(store_op, base, indices[2], rep_inputs[2],
                                      rep_nodes[3], control_input);
      rep_nodes[1] = graph()->NewNode(store_op, base, indices[1], rep_inputs[1],
                                      rep_nodes[2], control_input);
      rep_nodes[0]->ReplaceInput(3, rep_nodes[1]);

    } else {
      for (size_t i = 1; i < kMaxLanes; ++i) {
        rep_nodes[i] =
            graph()->NewNode(store_op, base, indices[i], rep_inputs[i]);
      }
    }

    ReplaceNode(node, rep_nodes);
  } else {
    DefaultLowering(node);
  }
}

void SimdScalarLowering::LowerBinaryOp(Node* node, SimdType input_rep_type,
                                       const Operator* op, bool invert_inputs) {
  DCHECK(node->InputCount() == 2);
  Node** rep_left = GetReplacementsWithType(node->InputAt(0), input_rep_type);
  Node** rep_right = GetReplacementsWithType(node->InputAt(1), input_rep_type);
  Node* rep_node[kMaxLanes];
  for (int i = 0; i < kMaxLanes; ++i) {
    if (invert_inputs) {
      rep_node[i] = graph()->NewNode(op, rep_right[i], rep_left[i]);
    } else {
      rep_node[i] = graph()->NewNode(op, rep_left[i], rep_right[i]);
    }
  }
  ReplaceNode(node, rep_node);
}

void SimdScalarLowering::LowerUnaryOp(Node* node, SimdType input_rep_type,
                                      const Operator* op) {
  DCHECK(node->InputCount() == 1);
  Node** rep = GetReplacementsWithType(node->InputAt(0), input_rep_type);
  Node* rep_node[kMaxLanes];
  for (int i = 0; i < kMaxLanes; ++i) {
    rep_node[i] = graph()->NewNode(op, rep[i]);
  }
  ReplaceNode(node, rep_node);
}

void SimdScalarLowering::LowerIntMinMax(Node* node, const Operator* op,
                                        bool is_max) {
  DCHECK(node->InputCount() == 2);
  Node** rep_left = GetReplacementsWithType(node->InputAt(0), SimdType::kInt32);
  Node** rep_right =
      GetReplacementsWithType(node->InputAt(1), SimdType::kInt32);
  Node* rep_node[kMaxLanes];
  for (int i = 0; i < kMaxLanes; ++i) {
    Diamond d(graph(), common(),
              graph()->NewNode(op, rep_left[i], rep_right[i]));
    if (is_max) {
      rep_node[i] =
          d.Phi(MachineRepresentation::kWord32, rep_right[i], rep_left[i]);
    } else {
      rep_node[i] =
          d.Phi(MachineRepresentation::kWord32, rep_left[i], rep_right[i]);
    }
  }
  ReplaceNode(node, rep_node);
}

Node* SimdScalarLowering::BuildF64Trunc(Node* input) {
  if (machine()->Float64RoundTruncate().IsSupported()) {
    return graph()->NewNode(machine()->Float64RoundTruncate().op(), input);
  } else {
    ExternalReference ref =
        ExternalReference::wasm_f64_trunc(jsgraph_->isolate());
    Node* stack_slot =
        graph()->NewNode(machine()->StackSlot(MachineRepresentation::kFloat64));
    const Operator* store_op = machine()->Store(
        StoreRepresentation(MachineRepresentation::kFloat64, kNoWriteBarrier));
    Node* effect =
        graph()->NewNode(store_op, stack_slot, jsgraph_->Int32Constant(0),
                         input, graph()->start(), graph()->start());
    Node* function = graph()->NewNode(common()->ExternalConstant(ref));
    Node** args = zone()->NewArray<Node*>(4);
    args[0] = function;
    args[1] = stack_slot;
    args[2] = effect;
    args[3] = graph()->start();
    Signature<MachineType>::Builder sig_builder(zone(), 0, 1);
    sig_builder.AddParam(MachineType::Pointer());
    CallDescriptor* desc =
        Linkage::GetSimplifiedCDescriptor(zone(), sig_builder.Build());
    Node* call = graph()->NewNode(common()->Call(desc), 4, args);
    return graph()->NewNode(machine()->Load(LoadRepresentation::Float64()),
                            stack_slot, jsgraph_->Int32Constant(0), call,
                            graph()->start());
  }
}

void SimdScalarLowering::LowerConvertFromFloat(Node* node, bool is_signed) {
  DCHECK(node->InputCount() == 1);
  Node** rep = GetReplacementsWithType(node->InputAt(0), SimdType::kFloat32);
  Node* rep_node[kMaxLanes];
  Node* double_zero = graph()->NewNode(common()->Float64Constant(0.0));
  Node* min = graph()->NewNode(
      common()->Float64Constant(static_cast<double>(is_signed ? kMinInt : 0)));
  Node* max = graph()->NewNode(common()->Float64Constant(
      static_cast<double>(is_signed ? kMaxInt : 0xffffffffu)));
  for (int i = 0; i < kMaxLanes; ++i) {
    Node* double_rep =
        graph()->NewNode(machine()->ChangeFloat32ToFloat64(), rep[i]);
    Diamond nan_d(graph(), common(), graph()->NewNode(machine()->Float64Equal(),
                                                      double_rep, double_rep));
    Node* temp =
        nan_d.Phi(MachineRepresentation::kFloat64, double_rep, double_zero);
    Diamond min_d(graph(), common(),
                  graph()->NewNode(machine()->Float64LessThan(), temp, min));
    temp = min_d.Phi(MachineRepresentation::kFloat64, min, temp);
    Diamond max_d(graph(), common(),
                  graph()->NewNode(machine()->Float64LessThan(), max, temp));
    temp = max_d.Phi(MachineRepresentation::kFloat64, max, temp);
    Node* trunc = BuildF64Trunc(temp);
    if (is_signed) {
      rep_node[i] = graph()->NewNode(machine()->ChangeFloat64ToInt32(), trunc);
    } else {
      rep_node[i] =
          graph()->NewNode(machine()->TruncateFloat64ToUint32(), trunc);
    }
  }
  ReplaceNode(node, rep_node);
}

void SimdScalarLowering::LowerShiftOp(Node* node, const Operator* op) {
  static int32_t shift_mask = 0x1f;
  DCHECK_EQ(1, node->InputCount());
  int32_t shift_amount = OpParameter<int32_t>(node);
  Node* shift_node =
      graph()->NewNode(common()->Int32Constant(shift_amount & shift_mask));
  Node** rep = GetReplacementsWithType(node->InputAt(0), SimdType::kInt32);
  Node* rep_node[kMaxLanes];
  for (int i = 0; i < kMaxLanes; ++i) {
    rep_node[i] = graph()->NewNode(op, rep[i], shift_node);
  }
  ReplaceNode(node, rep_node);
}

void SimdScalarLowering::LowerNotEqual(Node* node, SimdType input_rep_type,
                                       const Operator* op) {
  DCHECK(node->InputCount() == 2);
  Node** rep_left = GetReplacementsWithType(node->InputAt(0), input_rep_type);
  Node** rep_right = GetReplacementsWithType(node->InputAt(1), input_rep_type);
  Node* rep_node[kMaxLanes];
  for (int i = 0; i < kMaxLanes; ++i) {
    Diamond d(graph(), common(),
              graph()->NewNode(op, rep_left[i], rep_right[i]));
    rep_node[i] = d.Phi(MachineRepresentation::kWord32,
                        jsgraph_->Int32Constant(0), jsgraph_->Int32Constant(1));
  }
  ReplaceNode(node, rep_node);
}

void SimdScalarLowering::LowerNode(Node* node) {
  SimdType rep_type = ReplacementType(node);
  switch (node->opcode()) {
    case IrOpcode::kStart: {
      int parameter_count = GetParameterCountAfterLowering();
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
      DCHECK(node->InputCount() == 1);
      // Only exchange the node if the parameter count actually changed. We do
      // not even have to do the default lowering because the the start node,
      // the only input of a parameter node, only changes if the parameter count
      // changes.
      if (GetParameterCountAfterLowering() !=
          static_cast<int>(signature()->parameter_count())) {
        int old_index = ParameterIndexOf(node->op());
        int new_index = GetParameterIndexAfterLowering(signature(), old_index);
        if (old_index == new_index) {
          NodeProperties::ChangeOp(node, common()->Parameter(new_index));

          Node* new_node[kMaxLanes];
          for (int i = 0; i < kMaxLanes; ++i) {
            new_node[i] = nullptr;
          }
          new_node[0] = node;
          if (signature()->GetParam(old_index) ==
              MachineRepresentation::kSimd128) {
            for (int i = 1; i < kMaxLanes; ++i) {
              new_node[i] = graph()->NewNode(common()->Parameter(new_index + i),
                                             graph()->start());
            }
          }
          ReplaceNode(node, new_node);
        }
      }
      break;
    }
    case IrOpcode::kLoad: {
      MachineRepresentation rep =
          LoadRepresentationOf(node->op()).representation();
      const Operator* load_op;
      if (rep_type == SimdType::kInt32) {
        load_op = machine()->Load(MachineType::Int32());
      } else if (rep_type == SimdType::kFloat32) {
        load_op = machine()->Load(MachineType::Float32());
      }
      LowerLoadOp(rep, node, load_op);
      break;
    }
    case IrOpcode::kUnalignedLoad: {
      MachineRepresentation rep =
          UnalignedLoadRepresentationOf(node->op()).representation();
      const Operator* load_op;
      if (rep_type == SimdType::kInt32) {
        load_op = machine()->UnalignedLoad(MachineType::Int32());
      } else if (rep_type == SimdType::kFloat32) {
        load_op = machine()->UnalignedLoad(MachineType::Float32());
      }
      LowerLoadOp(rep, node, load_op);
      break;
    }
    case IrOpcode::kStore: {
      MachineRepresentation rep =
          StoreRepresentationOf(node->op()).representation();
      WriteBarrierKind write_barrier_kind =
          StoreRepresentationOf(node->op()).write_barrier_kind();
      const Operator* store_op;
      if (rep_type == SimdType::kInt32) {
        store_op = machine()->Store(StoreRepresentation(
            MachineRepresentation::kWord32, write_barrier_kind));
      } else {
        store_op = machine()->Store(StoreRepresentation(
            MachineRepresentation::kFloat32, write_barrier_kind));
      }
      LowerStoreOp(rep, node, store_op, rep_type);
      break;
    }
    case IrOpcode::kUnalignedStore: {
      MachineRepresentation rep = UnalignedStoreRepresentationOf(node->op());
      const Operator* store_op;
      if (rep_type == SimdType::kInt32) {
        store_op = machine()->UnalignedStore(MachineRepresentation::kWord32);
      } else {
        store_op = machine()->UnalignedStore(MachineRepresentation::kFloat32);
      }
      LowerStoreOp(rep, node, store_op, rep_type);
      break;
    }
    case IrOpcode::kReturn: {
      DefaultLowering(node);
      int new_return_count = GetReturnCountAfterLowering(signature());
      if (static_cast<int>(signature()->return_count()) != new_return_count) {
        NodeProperties::ChangeOp(node, common()->Return(new_return_count));
      }
      break;
    }
    case IrOpcode::kCall: {
      // TODO(turbofan): Make WASM code const-correct wrt. CallDescriptor.
      CallDescriptor* descriptor =
          const_cast<CallDescriptor*>(CallDescriptorOf(node->op()));
      if (DefaultLowering(node) ||
          (descriptor->ReturnCount() == 1 &&
           descriptor->GetReturnType(0) == MachineType::Simd128())) {
        // We have to adjust the call descriptor.
        const Operator* op =
            common()->Call(wasm::ModuleEnv::GetI32WasmCallDescriptorForSimd(
                zone(), descriptor));
        NodeProperties::ChangeOp(node, op);
      }
      if (descriptor->ReturnCount() == 1 &&
          descriptor->GetReturnType(0) == MachineType::Simd128()) {
        // We access the additional return values through projections.
        Node* rep_node[kMaxLanes];
        for (int i = 0; i < kMaxLanes; ++i) {
          rep_node[i] =
              graph()->NewNode(common()->Projection(i), node, graph()->start());
        }
        ReplaceNode(node, rep_node);
      }
      break;
    }
    case IrOpcode::kPhi: {
      MachineRepresentation rep = PhiRepresentationOf(node->op());
      if (rep == MachineRepresentation::kSimd128) {
        // The replacement nodes have already been created, we only have to
        // replace placeholder nodes.
        Node** rep_node = GetReplacements(node);
        for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
          Node** rep_input =
              GetReplacementsWithType(node->InputAt(i), rep_type);
          for (int j = 0; j < kMaxLanes; j++) {
            rep_node[j]->ReplaceInput(i, rep_input[j]);
          }
        }
      } else {
        DefaultLowering(node);
      }
      break;
    }
#define I32X4_BINOP_CASE(opcode, instruction)                \
  case IrOpcode::opcode: {                                   \
    LowerBinaryOp(node, rep_type, machine()->instruction()); \
    break;                                                   \
  }
      I32X4_BINOP_CASE(kI32x4Add, Int32Add)
      I32X4_BINOP_CASE(kI32x4Sub, Int32Sub)
      I32X4_BINOP_CASE(kI32x4Mul, Int32Mul)
      I32X4_BINOP_CASE(kS128And, Word32And)
      I32X4_BINOP_CASE(kS128Or, Word32Or)
      I32X4_BINOP_CASE(kS128Xor, Word32Xor)
#undef I32X4_BINOP_CASE
    case IrOpcode::kI32x4MaxS: {
      LowerIntMinMax(node, machine()->Int32LessThan(), true);
      break;
    }
    case IrOpcode::kI32x4MinS: {
      LowerIntMinMax(node, machine()->Int32LessThan(), false);
      break;
    }
    case IrOpcode::kI32x4MaxU: {
      LowerIntMinMax(node, machine()->Uint32LessThan(), true);
      break;
    }
    case IrOpcode::kI32x4MinU: {
      LowerIntMinMax(node, machine()->Uint32LessThan(), false);
      break;
    }
    case IrOpcode::kI32x4Neg: {
      DCHECK(node->InputCount() == 1);
      Node** rep = GetReplacementsWithType(node->InputAt(0), rep_type);
      Node* rep_node[kMaxLanes];
      Node* zero = graph()->NewNode(common()->Int32Constant(0));
      for (int i = 0; i < kMaxLanes; ++i) {
        rep_node[i] = graph()->NewNode(machine()->Int32Sub(), zero, rep[i]);
      }
      ReplaceNode(node, rep_node);
      break;
    }
    case IrOpcode::kS128Not: {
      DCHECK(node->InputCount() == 1);
      Node** rep = GetReplacementsWithType(node->InputAt(0), rep_type);
      Node* rep_node[kMaxLanes];
      Node* mask = graph()->NewNode(common()->Int32Constant(0xffffffff));
      for (int i = 0; i < kMaxLanes; ++i) {
        rep_node[i] = graph()->NewNode(machine()->Word32Xor(), rep[i], mask);
      }
      ReplaceNode(node, rep_node);
      break;
    }
    case IrOpcode::kI32x4SConvertF32x4: {
      LowerConvertFromFloat(node, true);
      break;
    }
    case IrOpcode::kI32x4UConvertF32x4: {
      LowerConvertFromFloat(node, false);
      break;
    }
    case IrOpcode::kI32x4Shl: {
      LowerShiftOp(node, machine()->Word32Shl());
      break;
    }
    case IrOpcode::kI32x4ShrS: {
      LowerShiftOp(node, machine()->Word32Sar());
      break;
    }
    case IrOpcode::kI32x4ShrU: {
      LowerShiftOp(node, machine()->Word32Shr());
      break;
    }
#define F32X4_BINOP_CASE(name)                                 \
  case IrOpcode::kF32x4##name: {                               \
    LowerBinaryOp(node, rep_type, machine()->Float32##name()); \
    break;                                                     \
  }
      F32X4_BINOP_CASE(Add)
      F32X4_BINOP_CASE(Sub)
      F32X4_BINOP_CASE(Mul)
      F32X4_BINOP_CASE(Div)
      F32X4_BINOP_CASE(Min)
      F32X4_BINOP_CASE(Max)
#undef F32X4_BINOP_CASE
#define F32X4_UNOP_CASE(name)                                 \
  case IrOpcode::kF32x4##name: {                              \
    LowerUnaryOp(node, rep_type, machine()->Float32##name()); \
    break;                                                    \
  }
      F32X4_UNOP_CASE(Abs)
      F32X4_UNOP_CASE(Neg)
      F32X4_UNOP_CASE(Sqrt)
#undef F32x4_UNOP_CASE
    case IrOpcode::kF32x4SConvertI32x4: {
      LowerUnaryOp(node, SimdType::kInt32, machine()->RoundInt32ToFloat32());
      break;
    }
    case IrOpcode::kF32x4UConvertI32x4: {
      LowerUnaryOp(node, SimdType::kInt32, machine()->RoundUint32ToFloat32());
      break;
    }
    case IrOpcode::kI32x4Splat:
    case IrOpcode::kF32x4Splat: {
      Node* rep_node[kMaxLanes];
      for (int i = 0; i < kMaxLanes; ++i) {
        if (HasReplacement(0, node->InputAt(0))) {
          rep_node[i] = GetReplacements(node->InputAt(0))[0];
        } else {
          rep_node[i] = node->InputAt(0);
        }
      }
      ReplaceNode(node, rep_node);
      break;
    }
    case IrOpcode::kI32x4ExtractLane:
    case IrOpcode::kF32x4ExtractLane: {
      int32_t lane = OpParameter<int32_t>(node);
      Node* rep_node[kMaxLanes] = {
          GetReplacementsWithType(node->InputAt(0), rep_type)[lane], nullptr,
          nullptr, nullptr};
      ReplaceNode(node, rep_node);
      break;
    }
    case IrOpcode::kI32x4ReplaceLane:
    case IrOpcode::kF32x4ReplaceLane: {
      DCHECK_EQ(2, node->InputCount());
      Node* repNode = node->InputAt(1);
      int32_t lane = OpParameter<int32_t>(node);
      DCHECK(lane >= 0 && lane <= 3);
      Node** rep_node = GetReplacementsWithType(node->InputAt(0), rep_type);
      if (HasReplacement(0, repNode)) {
        rep_node[lane] = GetReplacements(repNode)[0];
      } else {
        rep_node[lane] = repNode;
      }
      ReplaceNode(node, rep_node);
      break;
    }
#define COMPARISON_CASE(type, simd_op, lowering_op, invert)                   \
  case IrOpcode::simd_op: {                                                   \
    LowerBinaryOp(node, SimdType::k##type, machine()->lowering_op(), invert); \
    break;                                                                    \
  }
      COMPARISON_CASE(Float32, kF32x4Eq, Float32Equal, false)
      COMPARISON_CASE(Float32, kF32x4Lt, Float32LessThan, false)
      COMPARISON_CASE(Float32, kF32x4Le, Float32LessThanOrEqual, false)
      COMPARISON_CASE(Float32, kF32x4Gt, Float32LessThan, true)
      COMPARISON_CASE(Float32, kF32x4Ge, Float32LessThanOrEqual, true)
      COMPARISON_CASE(Int32, kI32x4Eq, Word32Equal, false)
      COMPARISON_CASE(Int32, kI32x4LtS, Int32LessThan, false)
      COMPARISON_CASE(Int32, kI32x4LeS, Int32LessThanOrEqual, false)
      COMPARISON_CASE(Int32, kI32x4GtS, Int32LessThan, true)
      COMPARISON_CASE(Int32, kI32x4GeS, Int32LessThanOrEqual, true)
      COMPARISON_CASE(Int32, kI32x4LtU, Uint32LessThan, false)
      COMPARISON_CASE(Int32, kI32x4LeU, Uint32LessThanOrEqual, false)
      COMPARISON_CASE(Int32, kI32x4GtU, Uint32LessThan, true)
      COMPARISON_CASE(Int32, kI32x4GeU, Uint32LessThanOrEqual, true)
#undef COMPARISON_CASE
    case IrOpcode::kF32x4Ne: {
      LowerNotEqual(node, SimdType::kFloat32, machine()->Float32Equal());
      break;
    }
    case IrOpcode::kI32x4Ne: {
      LowerNotEqual(node, SimdType::kInt32, machine()->Word32Equal());
      break;
    }
    case IrOpcode::kS32x4Select: {
      DCHECK(node->InputCount() == 3);
      DCHECK(ReplacementType(node->InputAt(0)) == SimdType::kSimd1x4);
      Node** boolean_input = GetReplacements(node->InputAt(0));
      Node** rep_left = GetReplacementsWithType(node->InputAt(1), rep_type);
      Node** rep_right = GetReplacementsWithType(node->InputAt(2), rep_type);
      Node* rep_node[kMaxLanes];
      for (int i = 0; i < kMaxLanes; ++i) {
        Diamond d(graph(), common(),
                  graph()->NewNode(machine()->Word32Equal(), boolean_input[i],
                                   jsgraph_->Int32Constant(0)));
        if (rep_type == SimdType::kFloat32) {
          rep_node[i] =
              d.Phi(MachineRepresentation::kFloat32, rep_right[1], rep_left[0]);
        } else if (rep_type == SimdType::kInt32) {
          rep_node[i] =
              d.Phi(MachineRepresentation::kWord32, rep_right[1], rep_left[0]);
        } else {
          UNREACHABLE();
        }
      }
      ReplaceNode(node, rep_node);
      break;
    }
    default: { DefaultLowering(node); }
  }
}

bool SimdScalarLowering::DefaultLowering(Node* node) {
  bool something_changed = false;
  for (int i = NodeProperties::PastValueIndex(node) - 1; i >= 0; i--) {
    Node* input = node->InputAt(i);
    if (HasReplacement(0, input)) {
      something_changed = true;
      node->ReplaceInput(i, GetReplacements(input)[0]);
    }
    if (HasReplacement(1, input)) {
      something_changed = true;
      for (int j = 1; j < kMaxLanes; j++) {
        node->InsertInput(zone(), i + j, GetReplacements(input)[j]);
      }
    }
  }
  return something_changed;
}

void SimdScalarLowering::ReplaceNode(Node* old, Node** new_node) {
  // if new_low == nullptr, then also new_high == nullptr.
  DCHECK(new_node[0] != nullptr ||
         (new_node[1] == nullptr && new_node[2] == nullptr &&
          new_node[3] == nullptr));
  for (int i = 0; i < kMaxLanes; ++i) {
    replacements_[old->id()].node[i] = new_node[i];
  }
}

bool SimdScalarLowering::HasReplacement(size_t index, Node* node) {
  return replacements_[node->id()].node[index] != nullptr;
}

SimdScalarLowering::SimdType SimdScalarLowering::ReplacementType(Node* node) {
  return replacements_[node->id()].type;
}

Node** SimdScalarLowering::GetReplacements(Node* node) {
  Node** result = replacements_[node->id()].node;
  DCHECK(result);
  return result;
}

Node** SimdScalarLowering::GetReplacementsWithType(Node* node, SimdType type) {
  Node** replacements = GetReplacements(node);
  if (ReplacementType(node) == type) {
    return GetReplacements(node);
  }
  Node** result = zone()->NewArray<Node*>(kMaxLanes);
  if (ReplacementType(node) == SimdType::kInt32 && type == SimdType::kFloat32) {
    for (int i = 0; i < kMaxLanes; ++i) {
      if (replacements[i] != nullptr) {
        result[i] = graph()->NewNode(machine()->BitcastInt32ToFloat32(),
                                     replacements[i]);
      } else {
        result[i] = nullptr;
      }
    }
  } else if (ReplacementType(node) == SimdType::kFloat32 &&
             type == SimdType::kInt32) {
    for (int i = 0; i < kMaxLanes; ++i) {
      if (replacements[i] != nullptr) {
        result[i] = graph()->NewNode(machine()->BitcastFloat32ToInt32(),
                                     replacements[i]);
      } else {
        result[i] = nullptr;
      }
    }
  } else {
    UNREACHABLE();
  }
  return result;
}

void SimdScalarLowering::PreparePhiReplacement(Node* phi) {
  MachineRepresentation rep = PhiRepresentationOf(phi->op());
  if (rep == MachineRepresentation::kSimd128) {
    // We have to create the replacements for a phi node before we actually
    // lower the phi to break potential cycles in the graph. The replacements of
    // input nodes do not exist yet, so we use a placeholder node to pass the
    // graph verifier.
    int value_count = phi->op()->ValueInputCount();
    SimdType type = ReplacementType(phi);
    Node** inputs_rep[kMaxLanes];
    for (int i = 0; i < kMaxLanes; ++i) {
      inputs_rep[i] = zone()->NewArray<Node*>(value_count + 1);
      inputs_rep[i][value_count] = NodeProperties::GetControlInput(phi, 0);
    }
    for (int i = 0; i < value_count; ++i) {
      for (int j = 0; j < kMaxLanes; j++) {
        inputs_rep[j][i] = placeholder_;
      }
    }
    Node* rep_nodes[kMaxLanes];
    for (int i = 0; i < kMaxLanes; ++i) {
      if (type == SimdType::kInt32) {
        rep_nodes[i] = graph()->NewNode(
            common()->Phi(MachineRepresentation::kWord32, value_count),
            value_count + 1, inputs_rep[i], false);
      } else if (type == SimdType::kFloat32) {
        rep_nodes[i] = graph()->NewNode(
            common()->Phi(MachineRepresentation::kFloat32, value_count),
            value_count + 1, inputs_rep[i], false);
      } else {
        UNREACHABLE();
      }
    }
    ReplaceNode(phi, rep_nodes);
  }
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
