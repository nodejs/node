// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simd-scalar-lowering.h"

#include "src/compiler/diamond.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/wasm-compiler.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {
static const int kNumLanes32 = 4;
static const int kNumLanes16 = 8;
static const int kNumLanes8 = 16;
static const int32_t kMask16 = 0xffff;
static const int32_t kMask8 = 0xff;
static const int32_t kShift16 = 16;
static const int32_t kShift8 = 24;
}  // anonymous

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
  replacements_[graph()->end()->id()].type = SimdType::kInt32x4;

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
  V(I32x4Shl)                     \
  V(I32x4ShrS)                    \
  V(I32x4Add)                     \
  V(I32x4Sub)                     \
  V(I32x4Mul)                     \
  V(I32x4MinS)                    \
  V(I32x4MaxS)                    \
  V(I32x4ShrU)                    \
  V(I32x4MinU)                    \
  V(I32x4MaxU)                    \
  V(I32x4Eq)                      \
  V(I32x4Ne)                      \
  V(I32x4LtS)                     \
  V(I32x4LeS)                     \
  V(I32x4GtS)                     \
  V(I32x4GeS)                     \
  V(I32x4LtU)                     \
  V(I32x4LeU)                     \
  V(I32x4GtU)                     \
  V(I32x4GeU)                     \
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
  V(F32x4Min)                       \
  V(F32x4Max)

#define FOREACH_FLOAT32X4_TO_INT32X4OPCODE(V) \
  V(F32x4Eq)                                  \
  V(F32x4Ne)                                  \
  V(F32x4Lt)                                  \
  V(F32x4Le)                                  \
  V(F32x4Gt)                                  \
  V(F32x4Ge)

#define FOREACH_INT16X8_OPCODE(V) \
  V(I16x8Splat)                   \
  V(I16x8ExtractLane)             \
  V(I16x8ReplaceLane)             \
  V(I16x8Neg)                     \
  V(I16x8Shl)                     \
  V(I16x8ShrS)                    \
  V(I16x8Add)                     \
  V(I16x8AddSaturateS)            \
  V(I16x8Sub)                     \
  V(I16x8SubSaturateS)            \
  V(I16x8Mul)                     \
  V(I16x8MinS)                    \
  V(I16x8MaxS)                    \
  V(I16x8ShrU)                    \
  V(I16x8AddSaturateU)            \
  V(I16x8SubSaturateU)            \
  V(I16x8MinU)                    \
  V(I16x8MaxU)                    \
  V(I16x8Eq)                      \
  V(I16x8Ne)                      \
  V(I16x8LtS)                     \
  V(I16x8LeS)                     \
  V(I16x8LtU)                     \
  V(I16x8LeU)

#define FOREACH_INT8X16_OPCODE(V) \
  V(I8x16Splat)                   \
  V(I8x16ExtractLane)             \
  V(I8x16ReplaceLane)             \
  V(I8x16Neg)                     \
  V(I8x16Shl)                     \
  V(I8x16ShrS)                    \
  V(I8x16Add)                     \
  V(I8x16AddSaturateS)            \
  V(I8x16Sub)                     \
  V(I8x16SubSaturateS)            \
  V(I8x16Mul)                     \
  V(I8x16MinS)                    \
  V(I8x16MaxS)                    \
  V(I8x16ShrU)                    \
  V(I8x16AddSaturateU)            \
  V(I8x16SubSaturateU)            \
  V(I8x16MinU)                    \
  V(I8x16MaxU)                    \
  V(I8x16Eq)                      \
  V(I8x16Ne)                      \
  V(I8x16LtS)                     \
  V(I8x16LeS)                     \
  V(I8x16LtU)                     \
  V(I8x16LeU)

MachineType SimdScalarLowering::MachineTypeFrom(SimdType simdType) {
  switch (simdType) {
    case SimdType::kFloat32x4:
      return MachineType::Float32();
    case SimdType::kInt32x4:
      return MachineType::Int32();
    case SimdType::kInt16x8:
      return MachineType::Int16();
    case SimdType::kInt8x16:
      return MachineType::Int8();
  }
  return MachineType::None();
}

void SimdScalarLowering::SetLoweredType(Node* node, Node* output) {
  switch (node->opcode()) {
#define CASE_STMT(name) case IrOpcode::k##name:
    FOREACH_INT32X4_OPCODE(CASE_STMT)
    case IrOpcode::kReturn:
    case IrOpcode::kParameter:
    case IrOpcode::kCall: {
      replacements_[node->id()].type = SimdType::kInt32x4;
      break;
    }
      FOREACH_FLOAT32X4_OPCODE(CASE_STMT) {
        replacements_[node->id()].type = SimdType::kFloat32x4;
        break;
      }
      FOREACH_FLOAT32X4_TO_INT32X4OPCODE(CASE_STMT) {
        replacements_[node->id()].type = SimdType::kInt32x4;
        break;
      }
      FOREACH_INT16X8_OPCODE(CASE_STMT) {
        replacements_[node->id()].type = SimdType::kInt16x8;
        break;
      }
      FOREACH_INT8X16_OPCODE(CASE_STMT) {
        replacements_[node->id()].type = SimdType::kInt8x16;
        break;
      }
    default: {
      switch (output->opcode()) {
        case IrOpcode::kF32x4SConvertI32x4:
        case IrOpcode::kF32x4UConvertI32x4: {
          replacements_[node->id()].type = SimdType::kInt32x4;
          break;
        }
          FOREACH_FLOAT32X4_TO_INT32X4OPCODE(CASE_STMT)
        case IrOpcode::kI32x4SConvertF32x4:
        case IrOpcode::kI32x4UConvertF32x4: {
          replacements_[node->id()].type = SimdType::kFloat32x4;
          break;
        }
        case IrOpcode::kS128Select: {
          replacements_[node->id()].type = SimdType::kInt32x4;
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

static int GetParameterIndexAfterLoweringSimd128(
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
    // GetParameterIndexAfterLoweringSimd128(parameter_count) returns the
    // parameter count after lowering.
    parameter_count_after_lowering_ = GetParameterIndexAfterLoweringSimd128(
        signature(), static_cast<int>(signature()->parameter_count()));
  }
  return parameter_count_after_lowering_;
}

static int GetReturnCountAfterLoweringSimd128(
    Signature<MachineRepresentation>* signature) {
  int result = static_cast<int>(signature->return_count());
  for (int i = 0; i < static_cast<int>(signature->return_count()); ++i) {
    if (signature->GetReturn(i) == MachineRepresentation::kSimd128) {
      result += 3;
    }
  }
  return result;
}

int SimdScalarLowering::NumLanes(SimdType type) {
  int num_lanes = 0;
  if (type == SimdType::kFloat32x4 || type == SimdType::kInt32x4) {
    num_lanes = kNumLanes32;
  } else if (type == SimdType::kInt16x8) {
    num_lanes = kNumLanes16;
  } else if (type == SimdType::kInt8x16) {
    num_lanes = kNumLanes8;
  } else {
    UNREACHABLE();
  }
  return num_lanes;
}

constexpr int SimdScalarLowering::kLaneOffsets[];

void SimdScalarLowering::GetIndexNodes(Node* index, Node** new_indices,
                                       SimdType type) {
  int num_lanes = NumLanes(type);
  int lane_width = kSimd128Size / num_lanes;
  int laneIndex = kLaneOffsets[0] / lane_width;
  new_indices[laneIndex] = index;
  for (int i = 1; i < num_lanes; ++i) {
    laneIndex = kLaneOffsets[i * lane_width] / lane_width;
    new_indices[laneIndex] = graph()->NewNode(
        machine()->Int32Add(), index,
        graph()->NewNode(
            common()->Int32Constant(static_cast<int>(i) * lane_width)));
  }
}

void SimdScalarLowering::LowerLoadOp(MachineRepresentation rep, Node* node,
                                     const Operator* load_op, SimdType type) {
  if (rep == MachineRepresentation::kSimd128) {
    Node* base = node->InputAt(0);
    Node* index = node->InputAt(1);
    int num_lanes = NumLanes(type);
    Node** indices = zone()->NewArray<Node*>(num_lanes);
    GetIndexNodes(index, indices, type);
    Node** rep_nodes = zone()->NewArray<Node*>(num_lanes);
    rep_nodes[0] = node;
    rep_nodes[0]->ReplaceInput(1, indices[0]);
    NodeProperties::ChangeOp(rep_nodes[0], load_op);
    if (node->InputCount() > 2) {
      DCHECK_LT(3, node->InputCount());
      Node* effect_input = node->InputAt(2);
      Node* control_input = node->InputAt(3);
      for (int i = num_lanes - 1; i > 0; --i) {
        rep_nodes[i] = graph()->NewNode(load_op, base, indices[i], effect_input,
                                        control_input);
        effect_input = rep_nodes[i];
      }
      rep_nodes[0]->ReplaceInput(2, rep_nodes[1]);
    } else {
      for (int i = 1; i < num_lanes; ++i) {
        rep_nodes[i] = graph()->NewNode(load_op, base, indices[i]);
      }
    }
    ReplaceNode(node, rep_nodes, num_lanes);
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
    int num_lanes = NumLanes(rep_type);
    Node** indices = zone()->NewArray<Node*>(num_lanes);
    GetIndexNodes(index, indices, rep_type);
    DCHECK_LT(2, node->InputCount());
    Node* value = node->InputAt(2);
    DCHECK(HasReplacement(1, value));
    Node** rep_nodes = zone()->NewArray<Node*>(num_lanes);
    rep_nodes[0] = node;
    Node** rep_inputs = GetReplacementsWithType(value, rep_type);
    rep_nodes[0]->ReplaceInput(2, rep_inputs[0]);
    rep_nodes[0]->ReplaceInput(1, indices[0]);
    NodeProperties::ChangeOp(node, store_op);
    if (node->InputCount() > 3) {
      DCHECK_LT(4, node->InputCount());
      Node* effect_input = node->InputAt(3);
      Node* control_input = node->InputAt(4);
      for (int i = num_lanes - 1; i > 0; --i) {
        rep_nodes[i] =
            graph()->NewNode(store_op, base, indices[i], rep_inputs[i],
                             effect_input, control_input);
        effect_input = rep_nodes[i];
      }
      rep_nodes[0]->ReplaceInput(3, rep_nodes[1]);
    } else {
      for (int i = 1; i < num_lanes; ++i) {
        rep_nodes[i] =
            graph()->NewNode(store_op, base, indices[i], rep_inputs[i]);
      }
    }
    ReplaceNode(node, rep_nodes, num_lanes);
  } else {
    DefaultLowering(node);
  }
}

void SimdScalarLowering::LowerBinaryOp(Node* node, SimdType input_rep_type,
                                       const Operator* op) {
  DCHECK_EQ(2, node->InputCount());
  Node** rep_left = GetReplacementsWithType(node->InputAt(0), input_rep_type);
  Node** rep_right = GetReplacementsWithType(node->InputAt(1), input_rep_type);
  int num_lanes = NumLanes(input_rep_type);
  Node** rep_node = zone()->NewArray<Node*>(num_lanes);
  for (int i = 0; i < num_lanes; ++i) {
    rep_node[i] = graph()->NewNode(op, rep_left[i], rep_right[i]);
  }
  ReplaceNode(node, rep_node, num_lanes);
}

void SimdScalarLowering::LowerCompareOp(Node* node, SimdType input_rep_type,
                                        const Operator* op,
                                        bool invert_inputs) {
  DCHECK_EQ(2, node->InputCount());
  Node** rep_left = GetReplacementsWithType(node->InputAt(0), input_rep_type);
  Node** rep_right = GetReplacementsWithType(node->InputAt(1), input_rep_type);
  int num_lanes = NumLanes(input_rep_type);
  Node** rep_node = zone()->NewArray<Node*>(num_lanes);
  for (int i = 0; i < num_lanes; ++i) {
    Node* cmp_result = nullptr;
    if (invert_inputs) {
      cmp_result = graph()->NewNode(op, rep_right[i], rep_left[i]);
    } else {
      cmp_result = graph()->NewNode(op, rep_left[i], rep_right[i]);
    }
    Diamond d_cmp(graph(), common(),
                  graph()->NewNode(machine()->Word32Equal(), cmp_result,
                                   jsgraph_->Int32Constant(0)));
    MachineRepresentation rep =
        (input_rep_type == SimdType::kFloat32x4)
            ? MachineRepresentation::kWord32
            : MachineTypeFrom(input_rep_type).representation();
    rep_node[i] =
        d_cmp.Phi(rep, jsgraph_->Int32Constant(0), jsgraph_->Int32Constant(-1));
  }
  ReplaceNode(node, rep_node, num_lanes);
}

Node* SimdScalarLowering::FixUpperBits(Node* input, int32_t shift) {
  return graph()->NewNode(machine()->Word32Sar(),
                          graph()->NewNode(machine()->Word32Shl(), input,
                                           jsgraph_->Int32Constant(shift)),
                          jsgraph_->Int32Constant(shift));
}

void SimdScalarLowering::LowerBinaryOpForSmallInt(Node* node,
                                                  SimdType input_rep_type,
                                                  const Operator* op) {
  DCHECK_EQ(2, node->InputCount());
  DCHECK(input_rep_type == SimdType::kInt16x8 ||
         input_rep_type == SimdType::kInt8x16);
  Node** rep_left = GetReplacementsWithType(node->InputAt(0), input_rep_type);
  Node** rep_right = GetReplacementsWithType(node->InputAt(1), input_rep_type);
  int num_lanes = NumLanes(input_rep_type);
  Node** rep_node = zone()->NewArray<Node*>(num_lanes);
  int32_t shift_val =
      (input_rep_type == SimdType::kInt16x8) ? kShift16 : kShift8;
  for (int i = 0; i < num_lanes; ++i) {
    rep_node[i] = FixUpperBits(graph()->NewNode(op, rep_left[i], rep_right[i]),
                               shift_val);
  }
  ReplaceNode(node, rep_node, num_lanes);
}

Node* SimdScalarLowering::Mask(Node* input, int32_t mask) {
  return graph()->NewNode(machine()->Word32And(), input,
                          jsgraph_->Int32Constant(mask));
}

void SimdScalarLowering::LowerSaturateBinaryOp(Node* node,
                                               SimdType input_rep_type,
                                               const Operator* op,
                                               bool is_signed) {
  DCHECK_EQ(2, node->InputCount());
  DCHECK(input_rep_type == SimdType::kInt16x8 ||
         input_rep_type == SimdType::kInt8x16);
  Node** rep_left = GetReplacementsWithType(node->InputAt(0), input_rep_type);
  Node** rep_right = GetReplacementsWithType(node->InputAt(1), input_rep_type);
  int32_t min = 0;
  int32_t max = 0;
  int32_t mask = 0;
  int32_t shift_val = 0;
  MachineRepresentation phi_rep;
  if (input_rep_type == SimdType::kInt16x8) {
    if (is_signed) {
      min = std::numeric_limits<int16_t>::min();
      max = std::numeric_limits<int16_t>::max();
    } else {
      min = std::numeric_limits<uint16_t>::min();
      max = std::numeric_limits<uint16_t>::max();
    }
    mask = kMask16;
    shift_val = kShift16;
    phi_rep = MachineRepresentation::kWord16;
  } else {
    if (is_signed) {
      min = std::numeric_limits<int8_t>::min();
      max = std::numeric_limits<int8_t>::max();
    } else {
      min = std::numeric_limits<uint8_t>::min();
      max = std::numeric_limits<uint8_t>::max();
    }
    mask = kMask8;
    shift_val = kShift8;
    phi_rep = MachineRepresentation::kWord8;
  }
  int num_lanes = NumLanes(input_rep_type);
  Node** rep_node = zone()->NewArray<Node*>(num_lanes);
  for (int i = 0; i < num_lanes; ++i) {
    Node* op_result = nullptr;
    Node* left = is_signed ? rep_left[i] : Mask(rep_left[i], mask);
    Node* right = is_signed ? rep_right[i] : Mask(rep_right[i], mask);
    op_result = graph()->NewNode(op, left, right);
    Diamond d_min(graph(), common(),
                  graph()->NewNode(machine()->Int32LessThan(), op_result,
                                   jsgraph_->Int32Constant(min)));
    rep_node[i] = d_min.Phi(phi_rep, jsgraph_->Int32Constant(min), op_result);
    Diamond d_max(graph(), common(),
                  graph()->NewNode(machine()->Int32LessThan(),
                                   jsgraph_->Int32Constant(max), rep_node[i]));
    rep_node[i] = d_max.Phi(phi_rep, jsgraph_->Int32Constant(max), rep_node[i]);
    rep_node[i] =
        is_signed ? rep_node[i] : FixUpperBits(rep_node[i], shift_val);
  }
  ReplaceNode(node, rep_node, num_lanes);
}

void SimdScalarLowering::LowerUnaryOp(Node* node, SimdType input_rep_type,
                                      const Operator* op) {
  DCHECK_EQ(1, node->InputCount());
  Node** rep = GetReplacementsWithType(node->InputAt(0), input_rep_type);
  int num_lanes = NumLanes(input_rep_type);
  Node** rep_node = zone()->NewArray<Node*>(num_lanes);
  for (int i = 0; i < num_lanes; ++i) {
    rep_node[i] = graph()->NewNode(op, rep[i]);
  }
  ReplaceNode(node, rep_node, num_lanes);
}

void SimdScalarLowering::LowerIntMinMax(Node* node, const Operator* op,
                                        bool is_max, SimdType type) {
  DCHECK_EQ(2, node->InputCount());
  Node** rep_left = GetReplacementsWithType(node->InputAt(0), type);
  Node** rep_right = GetReplacementsWithType(node->InputAt(1), type);
  int num_lanes = NumLanes(type);
  Node** rep_node = zone()->NewArray<Node*>(num_lanes);
  MachineRepresentation rep = MachineRepresentation::kNone;
  if (type == SimdType::kInt32x4) {
    rep = MachineRepresentation::kWord32;
  } else if (type == SimdType::kInt16x8) {
    rep = MachineRepresentation::kWord16;
  } else if (type == SimdType::kInt8x16) {
    rep = MachineRepresentation::kWord8;
  } else {
    UNREACHABLE();
  }
  for (int i = 0; i < num_lanes; ++i) {
    Diamond d(graph(), common(),
              graph()->NewNode(op, rep_left[i], rep_right[i]));
    if (is_max) {
      rep_node[i] = d.Phi(rep, rep_right[i], rep_left[i]);
    } else {
      rep_node[i] = d.Phi(rep, rep_left[i], rep_right[i]);
    }
  }
  ReplaceNode(node, rep_node, num_lanes);
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
  DCHECK_EQ(1, node->InputCount());
  Node** rep = GetReplacementsWithType(node->InputAt(0), SimdType::kFloat32x4);
  Node* rep_node[kNumLanes32];
  Node* double_zero = graph()->NewNode(common()->Float64Constant(0.0));
  Node* min = graph()->NewNode(
      common()->Float64Constant(static_cast<double>(is_signed ? kMinInt : 0)));
  Node* max = graph()->NewNode(common()->Float64Constant(
      static_cast<double>(is_signed ? kMaxInt : 0xffffffffu)));
  for (int i = 0; i < kNumLanes32; ++i) {
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
  ReplaceNode(node, rep_node, kNumLanes32);
}

void SimdScalarLowering::LowerShiftOp(Node* node, SimdType type) {
  DCHECK_EQ(1, node->InputCount());
  int32_t shift_amount = OpParameter<int32_t>(node);
  Node* shift_node = graph()->NewNode(common()->Int32Constant(shift_amount));
  Node** rep = GetReplacementsWithType(node->InputAt(0), type);
  int num_lanes = NumLanes(type);
  Node** rep_node = zone()->NewArray<Node*>(num_lanes);
  for (int i = 0; i < num_lanes; ++i) {
    rep_node[i] = rep[i];
    switch (node->opcode()) {
      case IrOpcode::kI8x16ShrU:
        rep_node[i] = Mask(rep_node[i], kMask8);
        rep_node[i] =
            graph()->NewNode(machine()->Word32Shr(), rep_node[i], shift_node);
        break;
      case IrOpcode::kI16x8ShrU:
        rep_node[i] = Mask(rep_node[i], kMask16);  // Fall through.
      case IrOpcode::kI32x4ShrU:
        rep_node[i] =
            graph()->NewNode(machine()->Word32Shr(), rep_node[i], shift_node);
        break;
      case IrOpcode::kI32x4Shl:
        rep_node[i] =
            graph()->NewNode(machine()->Word32Shl(), rep_node[i], shift_node);
        break;
      case IrOpcode::kI16x8Shl:
        rep_node[i] =
            graph()->NewNode(machine()->Word32Shl(), rep_node[i], shift_node);
        rep_node[i] = FixUpperBits(rep_node[i], kShift16);
        break;
      case IrOpcode::kI8x16Shl:
        rep_node[i] =
            graph()->NewNode(machine()->Word32Shl(), rep_node[i], shift_node);
        rep_node[i] = FixUpperBits(rep_node[i], kShift8);
        break;
      case IrOpcode::kI32x4ShrS:
      case IrOpcode::kI16x8ShrS:
      case IrOpcode::kI8x16ShrS:
        rep_node[i] =
            graph()->NewNode(machine()->Word32Sar(), rep_node[i], shift_node);
        break;
      default:
        UNREACHABLE();
    }
  }
  ReplaceNode(node, rep_node, num_lanes);
}

void SimdScalarLowering::LowerNotEqual(Node* node, SimdType input_rep_type,
                                       const Operator* op) {
  DCHECK_EQ(2, node->InputCount());
  Node** rep_left = GetReplacementsWithType(node->InputAt(0), input_rep_type);
  Node** rep_right = GetReplacementsWithType(node->InputAt(1), input_rep_type);
  int num_lanes = NumLanes(input_rep_type);
  Node** rep_node = zone()->NewArray<Node*>(num_lanes);
  for (int i = 0; i < num_lanes; ++i) {
    Diamond d(graph(), common(),
              graph()->NewNode(op, rep_left[i], rep_right[i]));
    MachineRepresentation rep =
        (input_rep_type == SimdType::kFloat32x4)
            ? MachineRepresentation::kWord32
            : MachineTypeFrom(input_rep_type).representation();
    rep_node[i] =
        d.Phi(rep, jsgraph_->Int32Constant(0), jsgraph_->Int32Constant(-1));
  }
  ReplaceNode(node, rep_node, num_lanes);
}

void SimdScalarLowering::LowerNode(Node* node) {
  SimdType rep_type = ReplacementType(node);
  int num_lanes = NumLanes(rep_type);
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
      DCHECK_EQ(1, node->InputCount());
      // Only exchange the node if the parameter count actually changed. We do
      // not even have to do the default lowering because the the start node,
      // the only input of a parameter node, only changes if the parameter count
      // changes.
      if (GetParameterCountAfterLowering() !=
          static_cast<int>(signature()->parameter_count())) {
        int old_index = ParameterIndexOf(node->op());
        int new_index =
            GetParameterIndexAfterLoweringSimd128(signature(), old_index);
        if (old_index == new_index) {
          NodeProperties::ChangeOp(node, common()->Parameter(new_index));

          Node* new_node[kNumLanes32];
          for (int i = 0; i < kNumLanes32; ++i) {
            new_node[i] = nullptr;
          }
          new_node[0] = node;
          if (signature()->GetParam(old_index) ==
              MachineRepresentation::kSimd128) {
            for (int i = 1; i < kNumLanes32; ++i) {
              new_node[i] = graph()->NewNode(common()->Parameter(new_index + i),
                                             graph()->start());
            }
          }
          ReplaceNode(node, new_node, kNumLanes32);
        }
      }
      break;
    }
    case IrOpcode::kLoad: {
      MachineRepresentation rep =
          LoadRepresentationOf(node->op()).representation();
      const Operator* load_op;
      load_op = machine()->Load(MachineTypeFrom(rep_type));
      LowerLoadOp(rep, node, load_op, rep_type);
      break;
    }
    case IrOpcode::kUnalignedLoad: {
      MachineRepresentation rep =
          UnalignedLoadRepresentationOf(node->op()).representation();
      const Operator* load_op;
      load_op = machine()->UnalignedLoad(MachineTypeFrom(rep_type));
      LowerLoadOp(rep, node, load_op, rep_type);
      break;
    }
    case IrOpcode::kStore: {
      MachineRepresentation rep =
          StoreRepresentationOf(node->op()).representation();
      WriteBarrierKind write_barrier_kind =
          StoreRepresentationOf(node->op()).write_barrier_kind();
      const Operator* store_op;
      store_op = machine()->Store(StoreRepresentation(
          MachineTypeFrom(rep_type).representation(), write_barrier_kind));
      LowerStoreOp(rep, node, store_op, rep_type);
      break;
    }
    case IrOpcode::kUnalignedStore: {
      MachineRepresentation rep = UnalignedStoreRepresentationOf(node->op());
      const Operator* store_op;
      store_op =
          machine()->UnalignedStore(MachineTypeFrom(rep_type).representation());
      LowerStoreOp(rep, node, store_op, rep_type);
      break;
    }
    case IrOpcode::kReturn: {
      DefaultLowering(node);
      int new_return_count = GetReturnCountAfterLoweringSimd128(signature());
      if (static_cast<int>(signature()->return_count()) != new_return_count) {
        NodeProperties::ChangeOp(node, common()->Return(new_return_count));
      }
      break;
    }
    case IrOpcode::kCall: {
      // TODO(turbofan): Make wasm code const-correct wrt. CallDescriptor.
      CallDescriptor* descriptor =
          const_cast<CallDescriptor*>(CallDescriptorOf(node->op()));
      if (DefaultLowering(node) ||
          (descriptor->ReturnCount() == 1 &&
           descriptor->GetReturnType(0) == MachineType::Simd128())) {
        // We have to adjust the call descriptor.
        const Operator* op =
            common()->Call(GetI32WasmCallDescriptorForSimd(zone(), descriptor));
        NodeProperties::ChangeOp(node, op);
      }
      if (descriptor->ReturnCount() == 1 &&
          descriptor->GetReturnType(0) == MachineType::Simd128()) {
        // We access the additional return values through projections.
        Node* rep_node[kNumLanes32];
        for (int i = 0; i < kNumLanes32; ++i) {
          rep_node[i] =
              graph()->NewNode(common()->Projection(i), node, graph()->start());
        }
        ReplaceNode(node, rep_node, kNumLanes32);
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
          for (int j = 0; j < num_lanes; j++) {
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
    case IrOpcode::kI16x8Add:
    case IrOpcode::kI8x16Add: {
      LowerBinaryOpForSmallInt(node, rep_type, machine()->Int32Add());
      break;
    }
    case IrOpcode::kI16x8Sub:
    case IrOpcode::kI8x16Sub: {
      LowerBinaryOpForSmallInt(node, rep_type, machine()->Int32Sub());
      break;
    }
    case IrOpcode::kI16x8Mul:
    case IrOpcode::kI8x16Mul: {
      LowerBinaryOpForSmallInt(node, rep_type, machine()->Int32Mul());
      break;
    }
    case IrOpcode::kI16x8AddSaturateS:
    case IrOpcode::kI8x16AddSaturateS: {
      LowerSaturateBinaryOp(node, rep_type, machine()->Int32Add(), true);
      break;
    }
    case IrOpcode::kI16x8SubSaturateS:
    case IrOpcode::kI8x16SubSaturateS: {
      LowerSaturateBinaryOp(node, rep_type, machine()->Int32Sub(), true);
      break;
    }
    case IrOpcode::kI16x8AddSaturateU:
    case IrOpcode::kI8x16AddSaturateU: {
      LowerSaturateBinaryOp(node, rep_type, machine()->Int32Add(), false);
      break;
    }
    case IrOpcode::kI16x8SubSaturateU:
    case IrOpcode::kI8x16SubSaturateU: {
      LowerSaturateBinaryOp(node, rep_type, machine()->Int32Sub(), false);
      break;
    }
    case IrOpcode::kI32x4MaxS:
    case IrOpcode::kI16x8MaxS:
    case IrOpcode::kI8x16MaxS: {
      LowerIntMinMax(node, machine()->Int32LessThan(), true, rep_type);
      break;
    }
    case IrOpcode::kI32x4MinS:
    case IrOpcode::kI16x8MinS:
    case IrOpcode::kI8x16MinS: {
      LowerIntMinMax(node, machine()->Int32LessThan(), false, rep_type);
      break;
    }
    case IrOpcode::kI32x4MaxU:
    case IrOpcode::kI16x8MaxU:
    case IrOpcode::kI8x16MaxU: {
      LowerIntMinMax(node, machine()->Uint32LessThan(), true, rep_type);
      break;
    }
    case IrOpcode::kI32x4MinU:
    case IrOpcode::kI16x8MinU:
    case IrOpcode::kI8x16MinU: {
      LowerIntMinMax(node, machine()->Uint32LessThan(), false, rep_type);
      break;
    }
    case IrOpcode::kI32x4Neg:
    case IrOpcode::kI16x8Neg:
    case IrOpcode::kI8x16Neg: {
      DCHECK_EQ(1, node->InputCount());
      Node** rep = GetReplacementsWithType(node->InputAt(0), rep_type);
      int num_lanes = NumLanes(rep_type);
      Node** rep_node = zone()->NewArray<Node*>(num_lanes);
      Node* zero = graph()->NewNode(common()->Int32Constant(0));
      for (int i = 0; i < num_lanes; ++i) {
        rep_node[i] = graph()->NewNode(machine()->Int32Sub(), zero, rep[i]);
        if (node->opcode() == IrOpcode::kI16x8Neg) {
          rep_node[i] = FixUpperBits(rep_node[i], kShift16);
        } else if (node->opcode() == IrOpcode::kI8x16Neg) {
          rep_node[i] = FixUpperBits(rep_node[i], kShift8);
        }
      }
      ReplaceNode(node, rep_node, num_lanes);
      break;
    }
    case IrOpcode::kS128Not: {
      DCHECK_EQ(1, node->InputCount());
      Node** rep = GetReplacementsWithType(node->InputAt(0), rep_type);
      Node* rep_node[kNumLanes32];
      Node* mask = graph()->NewNode(common()->Int32Constant(0xffffffff));
      for (int i = 0; i < kNumLanes32; ++i) {
        rep_node[i] = graph()->NewNode(machine()->Word32Xor(), rep[i], mask);
      }
      ReplaceNode(node, rep_node, kNumLanes32);
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
    case IrOpcode::kI32x4Shl:
    case IrOpcode::kI16x8Shl:
    case IrOpcode::kI8x16Shl:
    case IrOpcode::kI32x4ShrS:
    case IrOpcode::kI16x8ShrS:
    case IrOpcode::kI8x16ShrS:
    case IrOpcode::kI32x4ShrU:
    case IrOpcode::kI16x8ShrU:
    case IrOpcode::kI8x16ShrU: {
      LowerShiftOp(node, rep_type);
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
#undef F32x4_UNOP_CASE
    case IrOpcode::kF32x4SConvertI32x4: {
      LowerUnaryOp(node, SimdType::kInt32x4, machine()->RoundInt32ToFloat32());
      break;
    }
    case IrOpcode::kF32x4UConvertI32x4: {
      LowerUnaryOp(node, SimdType::kInt32x4, machine()->RoundUint32ToFloat32());
      break;
    }
    case IrOpcode::kI32x4Splat:
    case IrOpcode::kF32x4Splat:
    case IrOpcode::kI16x8Splat:
    case IrOpcode::kI8x16Splat: {
      Node** rep_node = zone()->NewArray<Node*>(num_lanes);
      for (int i = 0; i < num_lanes; ++i) {
        if (HasReplacement(0, node->InputAt(0))) {
          rep_node[i] = GetReplacements(node->InputAt(0))[0];
        } else {
          rep_node[i] = node->InputAt(0);
        }
      }
      ReplaceNode(node, rep_node, num_lanes);
      break;
    }
    case IrOpcode::kI32x4ExtractLane:
    case IrOpcode::kF32x4ExtractLane:
    case IrOpcode::kI16x8ExtractLane:
    case IrOpcode::kI8x16ExtractLane: {
      int32_t lane = OpParameter<int32_t>(node);
      Node** rep_node = zone()->NewArray<Node*>(num_lanes);
      rep_node[0] = GetReplacementsWithType(node->InputAt(0), rep_type)[lane];
      for (int i = 1; i < num_lanes; ++i) {
        rep_node[i] = nullptr;
      }
      ReplaceNode(node, rep_node, num_lanes);
      break;
    }
    case IrOpcode::kI32x4ReplaceLane:
    case IrOpcode::kF32x4ReplaceLane:
    case IrOpcode::kI16x8ReplaceLane:
    case IrOpcode::kI8x16ReplaceLane: {
      DCHECK_EQ(2, node->InputCount());
      Node* repNode = node->InputAt(1);
      int32_t lane = OpParameter<int32_t>(node);
      Node** rep_node = GetReplacementsWithType(node->InputAt(0), rep_type);
      if (HasReplacement(0, repNode)) {
        rep_node[lane] = GetReplacements(repNode)[0];
      } else {
        rep_node[lane] = repNode;
      }
      ReplaceNode(node, rep_node, num_lanes);
      break;
    }
#define COMPARISON_CASE(type, simd_op, lowering_op, invert)                    \
  case IrOpcode::simd_op: {                                                    \
    LowerCompareOp(node, SimdType::k##type, machine()->lowering_op(), invert); \
    break;                                                                     \
  }
      COMPARISON_CASE(Float32x4, kF32x4Eq, Float32Equal, false)
      COMPARISON_CASE(Float32x4, kF32x4Lt, Float32LessThan, false)
      COMPARISON_CASE(Float32x4, kF32x4Le, Float32LessThanOrEqual, false)
      COMPARISON_CASE(Float32x4, kF32x4Gt, Float32LessThan, true)
      COMPARISON_CASE(Float32x4, kF32x4Ge, Float32LessThanOrEqual, true)
      COMPARISON_CASE(Int32x4, kI32x4Eq, Word32Equal, false)
      COMPARISON_CASE(Int32x4, kI32x4LtS, Int32LessThan, false)
      COMPARISON_CASE(Int32x4, kI32x4LeS, Int32LessThanOrEqual, false)
      COMPARISON_CASE(Int32x4, kI32x4GtS, Int32LessThan, true)
      COMPARISON_CASE(Int32x4, kI32x4GeS, Int32LessThanOrEqual, true)
      COMPARISON_CASE(Int32x4, kI32x4LtU, Uint32LessThan, false)
      COMPARISON_CASE(Int32x4, kI32x4LeU, Uint32LessThanOrEqual, false)
      COMPARISON_CASE(Int32x4, kI32x4GtU, Uint32LessThan, true)
      COMPARISON_CASE(Int32x4, kI32x4GeU, Uint32LessThanOrEqual, true)
      COMPARISON_CASE(Int16x8, kI16x8Eq, Word32Equal, false)
      COMPARISON_CASE(Int16x8, kI16x8LtS, Int32LessThan, false)
      COMPARISON_CASE(Int16x8, kI16x8LeS, Int32LessThanOrEqual, false)
      COMPARISON_CASE(Int16x8, kI16x8GtS, Int32LessThan, true)
      COMPARISON_CASE(Int16x8, kI16x8GeS, Int32LessThanOrEqual, true)
      COMPARISON_CASE(Int16x8, kI16x8LtU, Uint32LessThan, false)
      COMPARISON_CASE(Int16x8, kI16x8LeU, Uint32LessThanOrEqual, false)
      COMPARISON_CASE(Int16x8, kI16x8GtU, Uint32LessThan, true)
      COMPARISON_CASE(Int16x8, kI16x8GeU, Uint32LessThanOrEqual, true)
      COMPARISON_CASE(Int8x16, kI8x16Eq, Word32Equal, false)
      COMPARISON_CASE(Int8x16, kI8x16LtS, Int32LessThan, false)
      COMPARISON_CASE(Int8x16, kI8x16LeS, Int32LessThanOrEqual, false)
      COMPARISON_CASE(Int8x16, kI8x16GtS, Int32LessThan, true)
      COMPARISON_CASE(Int8x16, kI8x16GeS, Int32LessThanOrEqual, true)
      COMPARISON_CASE(Int8x16, kI8x16LtU, Uint32LessThan, false)
      COMPARISON_CASE(Int8x16, kI8x16LeU, Uint32LessThanOrEqual, false)
      COMPARISON_CASE(Int8x16, kI8x16GtU, Uint32LessThan, true)
      COMPARISON_CASE(Int8x16, kI8x16GeU, Uint32LessThanOrEqual, true)
#undef COMPARISON_CASE
    case IrOpcode::kF32x4Ne: {
      LowerNotEqual(node, SimdType::kFloat32x4, machine()->Float32Equal());
      break;
    }
    case IrOpcode::kI32x4Ne: {
      LowerNotEqual(node, SimdType::kInt32x4, machine()->Word32Equal());
      break;
    }
    case IrOpcode::kI16x8Ne: {
      LowerNotEqual(node, SimdType::kInt16x8, machine()->Word32Equal());
      break;
    }
    case IrOpcode::kI8x16Ne: {
      LowerNotEqual(node, SimdType::kInt8x16, machine()->Word32Equal());
      break;
    }
    case IrOpcode::kS128Select: {
      DCHECK_EQ(3, node->InputCount());
      DCHECK(ReplacementType(node->InputAt(0)) == SimdType::kInt32x4 ||
             ReplacementType(node->InputAt(0)) == SimdType::kInt16x8 ||
             ReplacementType(node->InputAt(0)) == SimdType::kInt8x16);
      Node** boolean_input = GetReplacements(node->InputAt(0));
      Node** rep_left = GetReplacementsWithType(node->InputAt(1), rep_type);
      Node** rep_right = GetReplacementsWithType(node->InputAt(2), rep_type);
      Node** rep_node = zone()->NewArray<Node*>(num_lanes);
      for (int i = 0; i < num_lanes; ++i) {
        Diamond d(graph(), common(),
                  graph()->NewNode(machine()->Word32Equal(), boolean_input[i],
                                   jsgraph_->Int32Constant(0)));
        rep_node[i] = d.Phi(MachineTypeFrom(rep_type).representation(),
                            rep_right[1], rep_left[0]);
      }
      ReplaceNode(node, rep_node, num_lanes);
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
      for (int j = 1; j < ReplacementCount(input); ++j) {
        node->InsertInput(zone(), i + j, GetReplacements(input)[j]);
      }
    }
  }
  return something_changed;
}

void SimdScalarLowering::ReplaceNode(Node* old, Node** new_nodes, int count) {
  replacements_[old->id()].node = zone()->NewArray<Node*>(count);
  for (int i = 0; i < count; ++i) {
    replacements_[old->id()].node[i] = new_nodes[i];
  }
  replacements_[old->id()].num_replacements = count;
}

bool SimdScalarLowering::HasReplacement(size_t index, Node* node) {
  return replacements_[node->id()].node != nullptr &&
         replacements_[node->id()].node[index] != nullptr;
}

SimdScalarLowering::SimdType SimdScalarLowering::ReplacementType(Node* node) {
  return replacements_[node->id()].type;
}

Node** SimdScalarLowering::GetReplacements(Node* node) {
  Node** result = replacements_[node->id()].node;
  DCHECK(result);
  return result;
}

int SimdScalarLowering::ReplacementCount(Node* node) {
  return replacements_[node->id()].num_replacements;
}

void SimdScalarLowering::Int32ToFloat32(Node** replacements, Node** result) {
  for (int i = 0; i < kNumLanes32; ++i) {
    if (replacements[i] != nullptr) {
      result[i] =
          graph()->NewNode(machine()->BitcastInt32ToFloat32(), replacements[i]);
    } else {
      result[i] = nullptr;
    }
  }
}

void SimdScalarLowering::Float32ToInt32(Node** replacements, Node** result) {
  for (int i = 0; i < kNumLanes32; ++i) {
    if (replacements[i] != nullptr) {
      result[i] =
          graph()->NewNode(machine()->BitcastFloat32ToInt32(), replacements[i]);
    } else {
      result[i] = nullptr;
    }
  }
}

Node** SimdScalarLowering::GetReplacementsWithType(Node* node, SimdType type) {
  Node** replacements = GetReplacements(node);
  if (ReplacementType(node) == type) {
    return GetReplacements(node);
  }
  int num_lanes = NumLanes(type);
  Node** result = zone()->NewArray<Node*>(num_lanes);
  if (type == SimdType::kInt32x4) {
    if (ReplacementType(node) == SimdType::kFloat32x4) {
      Float32ToInt32(replacements, result);
    } else if (ReplacementType(node) == SimdType::kInt16x8) {
      UNIMPLEMENTED();
    } else {
      UNREACHABLE();
    }
  } else if (type == SimdType::kFloat32x4) {
    if (ReplacementType(node) == SimdType::kInt32x4) {
      Int32ToFloat32(replacements, result);
    } else if (ReplacementType(node) == SimdType::kInt16x8) {
      UNIMPLEMENTED();
    } else {
      UNREACHABLE();
    }
  } else if (type == SimdType::kInt16x8) {
    if (ReplacementType(node) == SimdType::kInt32x4 ||
        ReplacementType(node) == SimdType::kFloat32x4) {
      UNIMPLEMENTED();
    } else {
      UNREACHABLE();
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
    int num_lanes = NumLanes(type);
    Node*** inputs_rep = zone()->NewArray<Node**>(num_lanes);
    for (int i = 0; i < num_lanes; ++i) {
      inputs_rep[i] = zone()->NewArray<Node*>(value_count + 1);
      inputs_rep[i][value_count] = NodeProperties::GetControlInput(phi, 0);
    }
    for (int i = 0; i < value_count; ++i) {
      for (int j = 0; j < num_lanes; ++j) {
        inputs_rep[j][i] = placeholder_;
      }
    }
    Node** rep_nodes = zone()->NewArray<Node*>(num_lanes);
    for (int i = 0; i < num_lanes; ++i) {
      rep_nodes[i] = graph()->NewNode(
          common()->Phi(MachineTypeFrom(type).representation(), value_count),
          value_count + 1, inputs_rep[i], false);
    }
    ReplaceNode(phi, rep_nodes, num_lanes);
  }
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
