// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/graph-builder.h"

#include <limits>
#include <numeric>
#include <string_view>

#include "src/base/container-utils.h"
#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/safe_conversions.h"
#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/state-values-utils.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/deopt-data.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

namespace {

struct GraphBuilder {
  Isolate* isolate;
  JSHeapBroker* broker;
  Zone* graph_zone;
  Zone* phase_zone;
  Schedule& schedule;
  Assembler<reducer_list<>> assembler;
  Linkage* linkage;
  SourcePositionTable* source_positions;
  NodeOriginTable* origins;

  struct BlockData {
    Block* block;
  };
  NodeAuxData<OpIndex> op_mapping{phase_zone};
  ZoneVector<BlockData> block_mapping{schedule.RpoBlockCount(), phase_zone};

  base::Optional<BailoutReason> Run();
  Assembler<reducer_list<>>& Asm() { return assembler; }

 private:
  OpIndex Map(Node* old_node) {
    OpIndex result = op_mapping.Get(old_node);
    DCHECK(__ output_graph().IsValid(result));
    return result;
  }
  Block* Map(BasicBlock* block) {
    Block* result = block_mapping[block->rpo_number()].block;
    DCHECK_NOT_NULL(result);
    return result;
  }

  void FixLoopPhis(Block* loop, Block* backedge) {
    DCHECK(loop->IsLoop());
    for (Operation& op : __ output_graph().operations(*loop)) {
      if (!op.Is<PendingLoopPhiOp>()) continue;
      auto& pending_phi = op.Cast<PendingLoopPhiOp>();
      __ output_graph().Replace<PhiOp>(
          __ output_graph().Index(pending_phi),
          base::VectorOf(
              {pending_phi.first(), Map(pending_phi.data.old_backedge_node)}),
          pending_phi.rep);
    }
  }

  void ProcessDeoptInput(FrameStateData::Builder* builder, Node* input,
                         MachineType type) {
    DCHECK_NE(input->opcode(), IrOpcode::kObjectState);
    DCHECK_NE(input->opcode(), IrOpcode::kStateValues);
    DCHECK_NE(input->opcode(), IrOpcode::kTypedStateValues);
    if (input->opcode() == IrOpcode::kObjectId) {
      builder->AddDematerializedObjectReference(ObjectIdOf(input->op()));
    } else if (input->opcode() == IrOpcode::kTypedObjectState) {
      const TypedObjectStateInfo& info =
          OpParameter<TypedObjectStateInfo>(input->op());
      int field_count = input->op()->ValueInputCount();
      builder->AddDematerializedObject(info.object_id(),
                                       static_cast<uint32_t>(field_count));
      for (int i = 0; i < field_count; ++i) {
        ProcessDeoptInput(builder, input->InputAt(i),
                          (*info.machine_types())[i]);
      }
    } else if (input->opcode() == IrOpcode::kArgumentsElementsState) {
      builder->AddArgumentsElements(ArgumentsStateTypeOf(input->op()));
    } else if (input->opcode() == IrOpcode::kArgumentsLengthState) {
      builder->AddArgumentsLength();
    } else {
      builder->AddInput(type, Map(input));
    }
  }

  void ProcessStateValues(FrameStateData::Builder* builder,
                          Node* state_values) {
    for (auto it = StateValuesAccess(state_values).begin(); !it.done(); ++it) {
      if (Node* node = it.node()) {
        ProcessDeoptInput(builder, node, (*it).type);
      } else {
        builder->AddUnusedRegister();
      }
    }
  }

  void BuildFrameStateData(FrameStateData::Builder* builder,
                           FrameState frame_state) {
    if (frame_state.outer_frame_state()->opcode() != IrOpcode::kStart) {
      builder->AddParentFrameState(Map(frame_state.outer_frame_state()));
    }
    ProcessStateValues(builder, frame_state.parameters());
    ProcessStateValues(builder, frame_state.locals());
    ProcessStateValues(builder, frame_state.stack());
    ProcessDeoptInput(builder, frame_state.context(), MachineType::AnyTagged());
    ProcessDeoptInput(builder, frame_state.function(),
                      MachineType::AnyTagged());
  }

  Block::Kind BlockKind(BasicBlock* block) {
    switch (block->front()->opcode()) {
      case IrOpcode::kStart:
      case IrOpcode::kEnd:
      case IrOpcode::kMerge:
        return Block::Kind::kMerge;
      case IrOpcode::kIfTrue:
      case IrOpcode::kIfFalse:
      case IrOpcode::kIfValue:
      case IrOpcode::kIfDefault:
      case IrOpcode::kIfSuccess:
      case IrOpcode::kIfException:
        return Block::Kind::kBranchTarget;
      case IrOpcode::kLoop:
        return Block::Kind::kLoopHeader;
      default:
        block->front()->Print();
        UNIMPLEMENTED();
    }
  }
  V<Word32> BuildUint32Mod(V<Word32> lhs, V<Word32> rhs);
  OpIndex Process(Node* node, BasicBlock* block,
                  const base::SmallVector<int, 16>& predecessor_permutation,
                  OpIndex& dominating_frame_state,
                  base::Optional<BailoutReason>* bailout,
                  bool is_final_control = false);

  OpIndex EmitProjectionsAndTuple(OpIndex op_idx) {
    Operation& op = __ output_graph().Get(op_idx);
    base::Vector<const RegisterRepresentation> outputs_rep = op.outputs_rep();
    if (outputs_rep.size() <= 1) {
      // If {op} has a single output, there is no need to emit Projections or
      // Tuple, so we just return it.
      return op_idx;
    }
    base::SmallVector<OpIndex, 16> tuple_inputs;
    for (size_t i = 0; i < outputs_rep.size(); i++) {
      tuple_inputs.push_back(__ Projection(op_idx, i, outputs_rep[i]));
    }
    return __ Tuple(base::VectorOf(tuple_inputs));
  }
};

base::Optional<BailoutReason> GraphBuilder::Run() {
  for (BasicBlock* block : *schedule.rpo_order()) {
    block_mapping[block->rpo_number()].block =
        block->IsLoopHeader() ? __ NewLoopHeader() : __ NewBlock();
  }

  for (BasicBlock* block : *schedule.rpo_order()) {
    Block* target_block = Map(block);
    if (!__ Bind(target_block)) continue;

    // Since we visit blocks in rpo-order, the new block predecessors are sorted
    // in rpo order too. However, the input schedule does not order
    // predecessors, so we have to apply a corresponding permutation to phi
    // inputs.
    const BasicBlockVector& predecessors = block->predecessors();
    base::SmallVector<int, 16> predecessor_permutation(predecessors.size());
    std::iota(predecessor_permutation.begin(), predecessor_permutation.end(),
              0);
    std::sort(predecessor_permutation.begin(), predecessor_permutation.end(),
              [&](size_t i, size_t j) {
                return predecessors[i]->rpo_number() <
                       predecessors[j]->rpo_number();
              });

    OpIndex dominating_frame_state = OpIndex::Invalid();
    base::Optional<BailoutReason> bailout = base::nullopt;
    for (Node* node : *block->nodes()) {
      if (V8_UNLIKELY(node->InputCount() >=
                      int{std::numeric_limits<
                          decltype(Operation::input_count)>::max()})) {
        return BailoutReason::kTooManyArguments;
      }
      OpIndex i = Process(node, block, predecessor_permutation,
                          dominating_frame_state, &bailout);
      if (V8_UNLIKELY(bailout)) return bailout;
      op_mapping.Set(node, i);
    }
    if (Node* node = block->control_input()) {
      if (V8_UNLIKELY(node->InputCount() >=
                      int{std::numeric_limits<
                          decltype(Operation::input_count)>::max()})) {
        return BailoutReason::kTooManyArguments;
      }
      OpIndex i = Process(node, block, predecessor_permutation,
                          dominating_frame_state, &bailout, true);
      if (V8_UNLIKELY(bailout)) return bailout;
      op_mapping.Set(node, i);
    }
    switch (block->control()) {
      case BasicBlock::kGoto: {
        DCHECK_EQ(block->SuccessorCount(), 1);
        Block* destination = Map(block->SuccessorAt(0));
        __ Goto(destination);
        if (destination->IsBound()) {
          DCHECK(destination->IsLoop());
          FixLoopPhis(destination, target_block);
        }
        break;
      }
      case BasicBlock::kBranch:
      case BasicBlock::kSwitch:
      case BasicBlock::kReturn:
      case BasicBlock::kDeoptimize:
      case BasicBlock::kThrow:
      case BasicBlock::kCall:
      case BasicBlock::kTailCall:
        break;
      case BasicBlock::kNone:
        UNREACHABLE();
    }
    DCHECK_NULL(__ current_block());
  }

  if (source_positions->IsEnabled()) {
    for (OpIndex index : __ output_graph().AllOperationIndices()) {
      compiler::NodeId origin =
          __ output_graph().operation_origins()[index].DecodeTurbofanNodeId();
      __ output_graph().source_positions()[index] =
          source_positions->GetSourcePosition(origin);
    }
  }

  if (origins) {
    for (OpIndex index : __ output_graph().AllOperationIndices()) {
      OpIndex origin = __ output_graph().operation_origins()[index];
      origins->SetNodeOrigin(index.id(), origin.DecodeTurbofanNodeId());
    }
  }

  return base::nullopt;
}

V<Word32> GraphBuilder::BuildUint32Mod(V<Word32> lhs, V<Word32> rhs) {
  Label<Word32> done(this);

  // Compute the mask for the {rhs}.
  V<Word32> msk = __ Word32Sub(rhs, 1);

  // Check if the {rhs} is a power of two.
  IF(__ Word32Equal(__ Word32BitwiseAnd(rhs, msk), 0)) {
    // The {rhs} is a power of two, just do a fast bit masking.
    GOTO(done, __ Word32BitwiseAnd(lhs, msk));
  }
  ELSE {
    // The {rhs} is not a power of two, do a generic Uint32Mod.
    GOTO(done, __ Uint32Mod(lhs, rhs));
  }

  BIND(done, result);
  return result;
}

OpIndex GraphBuilder::Process(
    Node* node, BasicBlock* block,
    const base::SmallVector<int, 16>& predecessor_permutation,
    OpIndex& dominating_frame_state, base::Optional<BailoutReason>* bailout,
    bool is_final_control) {
  if (Asm().current_block() == nullptr) {
    return OpIndex::Invalid();
  }
  __ SetCurrentOrigin(OpIndex::EncodeTurbofanNodeId(node->id()));
  const Operator* op = node->op();
  Operator::Opcode opcode = op->opcode();
  switch (opcode) {
    case IrOpcode::kStart:
    case IrOpcode::kMerge:
    case IrOpcode::kLoop:
    case IrOpcode::kIfTrue:
    case IrOpcode::kIfFalse:
    case IrOpcode::kIfDefault:
    case IrOpcode::kIfValue:
    case IrOpcode::kStateValues:
    case IrOpcode::kTypedStateValues:
    case IrOpcode::kObjectId:
    case IrOpcode::kTypedObjectState:
    case IrOpcode::kArgumentsElementsState:
    case IrOpcode::kArgumentsLengthState:
    case IrOpcode::kEffectPhi:
    case IrOpcode::kTerminate:
      return OpIndex::Invalid();

    case IrOpcode::kCheckpoint: {
      // Preserve the frame state from this checkpoint for following nodes.
      dominating_frame_state = Map(NodeProperties::GetFrameStateInput(node));
      return OpIndex::Invalid();
    }

    case IrOpcode::kIfException: {
      return __ LoadException();
    }

    case IrOpcode::kIfSuccess: {
      // We emit all of the value projections of the call now, emit a Tuple with
      // all of those projections, and remap the old call to this new Tuple
      // instead of the CallAndCatchExceptionOp.
      Node* call = node->InputAt(0);
      DCHECK_EQ(call->opcode(), IrOpcode::kCall);
      OpIndex call_idx = Map(call);
      CallAndCatchExceptionOp& op =
          __ output_graph().Get(call_idx).Cast<CallAndCatchExceptionOp>();

      size_t return_count = op.outputs_rep().size();
      DCHECK_EQ(return_count, op.descriptor->descriptor->ReturnCount());
      if (return_count <= 1) {
        // Calls with one output (or zero) do not require Projections.
        return OpIndex::Invalid();
      }
      base::Vector<OpIndex> projections =
          graph_zone->NewVector<OpIndex>(return_count);
      for (size_t i = 0; i < return_count; i++) {
        projections[i] = __ Projection(call_idx, i, op.outputs_rep()[i]);
      }
      OpIndex tuple_idx = __ Tuple(projections);

      // Re-mapping {call} to {tuple_idx} so that subsequent projections are not
      // emitted.
      op_mapping.Set(call, tuple_idx);

      return OpIndex::Invalid();
    }

    case IrOpcode::kParameter: {
      const ParameterInfo& info = ParameterInfoOf(op);
      RegisterRepresentation rep =
          RegisterRepresentation::FromMachineRepresentation(
              linkage->GetParameterType(ParameterIndexOf(node->op()))
                  .representation());
      return __ Parameter(info.index(), rep, info.debug_name());
    }

    case IrOpcode::kOsrValue: {
      return __ OsrValue(OsrValueIndexOf(op));
    }

    case IrOpcode::kPhi: {
      int input_count = op->ValueInputCount();
      RegisterRepresentation rep =
          RegisterRepresentation::FromMachineRepresentation(
              PhiRepresentationOf(op));
      if (__ current_block()->IsLoop()) {
        DCHECK_EQ(input_count, 2);
        return __ PendingLoopPhi(Map(node->InputAt(0)), rep, node->InputAt(1));
      } else {
        base::SmallVector<OpIndex, 16> inputs;
        for (int i = 0; i < input_count; ++i) {
          inputs.push_back(Map(node->InputAt(predecessor_permutation[i])));
        }
        return __ Phi(base::VectorOf(inputs), rep);
      }
    }

    case IrOpcode::kInt64Constant:
      return __ Word64Constant(static_cast<uint64_t>(OpParameter<int64_t>(op)));
    case IrOpcode::kInt32Constant:
      return __ Word32Constant(static_cast<uint32_t>(OpParameter<int32_t>(op)));
    case IrOpcode::kFloat64Constant:
      return __ Float64Constant(OpParameter<double>(op));
    case IrOpcode::kFloat32Constant:
      return __ Float32Constant(OpParameter<float>(op));
    case IrOpcode::kNumberConstant:
      return __ NumberConstant(OpParameter<double>(op));
    case IrOpcode::kTaggedIndexConstant:
      return __ TaggedIndexConstant(OpParameter<int32_t>(op));
    case IrOpcode::kHeapConstant:
      return __ HeapConstant(HeapConstantOf(op));
    case IrOpcode::kCompressedHeapConstant:
      return __ CompressedHeapConstant(HeapConstantOf(op));
    case IrOpcode::kExternalConstant:
      return __ ExternalConstant(OpParameter<ExternalReference>(op));
    case IrOpcode::kRelocatableInt64Constant:
      return __ RelocatableConstant(
          OpParameter<RelocatablePtrConstantInfo>(op).value(),
          OpParameter<RelocatablePtrConstantInfo>(op).rmode());
#define BINOP_CASE(opcode, assembler_op) \
  case IrOpcode::k##opcode:              \
    return __ assembler_op(Map(node->InputAt(0)), Map(node->InputAt(1)));

      BINOP_CASE(Int32Add, Word32Add)
      BINOP_CASE(Int64Add, Word64Add)
      BINOP_CASE(Int32Mul, Word32Mul)
      BINOP_CASE(Int64Mul, Word64Mul)
      BINOP_CASE(Word32And, Word32BitwiseAnd)
      BINOP_CASE(Word64And, Word64BitwiseAnd)
      BINOP_CASE(Word32Or, Word32BitwiseOr)
      BINOP_CASE(Word64Or, Word64BitwiseOr)
      BINOP_CASE(Word32Xor, Word32BitwiseXor)
      BINOP_CASE(Word64Xor, Word64BitwiseXor)
      BINOP_CASE(Int32Sub, Word32Sub)
      BINOP_CASE(Int64Sub, Word64Sub)
      BINOP_CASE(Int32Div, Int32Div)
      BINOP_CASE(Uint32Div, Uint32Div)
      BINOP_CASE(Int64Div, Int64Div)
      BINOP_CASE(Uint64Div, Uint64Div)
      BINOP_CASE(Int32Mod, Int32Mod)
      BINOP_CASE(Uint32Mod, Uint32Mod)
      BINOP_CASE(Int64Mod, Int64Mod)
      BINOP_CASE(Uint64Mod, Uint64Mod)
      BINOP_CASE(Int32MulHigh, Int32MulOverflownBits)
      BINOP_CASE(Int64MulHigh, Int64MulOverflownBits)
      BINOP_CASE(Uint32MulHigh, Uint32MulOverflownBits)
      BINOP_CASE(Uint64MulHigh, Uint64MulOverflownBits)

      BINOP_CASE(Float32Add, Float32Add)
      BINOP_CASE(Float64Add, Float64Add)
      BINOP_CASE(Float32Sub, Float32Sub)
      BINOP_CASE(Float64Sub, Float64Sub)
      BINOP_CASE(Float64Mul, Float64Mul)
      BINOP_CASE(Float32Mul, Float32Mul)
      BINOP_CASE(Float32Div, Float32Div)
      BINOP_CASE(Float64Div, Float64Div)
      BINOP_CASE(Float32Min, Float32Min)
      BINOP_CASE(Float64Min, Float64Min)
      BINOP_CASE(Float32Max, Float32Max)
      BINOP_CASE(Float64Max, Float64Max)
      BINOP_CASE(Float64Mod, Float64Mod)
      BINOP_CASE(Float64Pow, Float64Power)
      BINOP_CASE(Float64Atan2, Float64Atan2)

      BINOP_CASE(Word32Shr, Word32ShiftRightLogical)
      BINOP_CASE(Word64Shr, Word64ShiftRightLogical)

      BINOP_CASE(Word32Shl, Word32ShiftLeft)
      BINOP_CASE(Word64Shl, Word64ShiftLeft)

      BINOP_CASE(Word32Rol, Word32RotateLeft)
      BINOP_CASE(Word64Rol, Word64RotateLeft)

      BINOP_CASE(Word32Ror, Word32RotateRight)
      BINOP_CASE(Word64Ror, Word64RotateRight)

      BINOP_CASE(Word32Equal, Word32Equal)
      BINOP_CASE(Word64Equal, Word64Equal)
      BINOP_CASE(Float32Equal, Float32Equal)
      BINOP_CASE(Float64Equal, Float64Equal)

      BINOP_CASE(Int32LessThan, Int32LessThan)
      BINOP_CASE(Int64LessThan, Int64LessThan)
      BINOP_CASE(Uint32LessThan, Uint32LessThan)
      BINOP_CASE(Uint64LessThan, Uint64LessThan)
      BINOP_CASE(Float32LessThan, Float32LessThan)
      BINOP_CASE(Float64LessThan, Float64LessThan)

      BINOP_CASE(Int32LessThanOrEqual, Int32LessThanOrEqual)
      BINOP_CASE(Int64LessThanOrEqual, Int64LessThanOrEqual)
      BINOP_CASE(Uint32LessThanOrEqual, Uint32LessThanOrEqual)
      BINOP_CASE(Uint64LessThanOrEqual, Uint64LessThanOrEqual)
      BINOP_CASE(Float32LessThanOrEqual, Float32LessThanOrEqual)
      BINOP_CASE(Float64LessThanOrEqual, Float64LessThanOrEqual)
#undef BINOP_CASE

#define TUPLE_BINOP_CASE(opcode, assembler_op)                         \
  case IrOpcode::k##opcode: {                                          \
    OpIndex idx =                                                      \
        __ assembler_op(Map(node->InputAt(0)), Map(node->InputAt(1))); \
    return EmitProjectionsAndTuple(idx);                               \
  }
      TUPLE_BINOP_CASE(Int32AddWithOverflow, Int32AddCheckOverflow)
      TUPLE_BINOP_CASE(Int64AddWithOverflow, Int64AddCheckOverflow)
      TUPLE_BINOP_CASE(Int32MulWithOverflow, Int32MulCheckOverflow)
      TUPLE_BINOP_CASE(Int64MulWithOverflow, Int64MulCheckOverflow)
      TUPLE_BINOP_CASE(Int32SubWithOverflow, Int32SubCheckOverflow)
      TUPLE_BINOP_CASE(Int64SubWithOverflow, Int64SubCheckOverflow)
#undef TUPLE_BINOP_CASE

    case IrOpcode::kWord64Sar:
    case IrOpcode::kWord32Sar: {
      WordRepresentation rep = opcode == IrOpcode::kWord64Sar
                                   ? WordRepresentation::Word64()
                                   : WordRepresentation::Word32();
      ShiftOp::Kind kind;
      switch (ShiftKindOf(op)) {
        case ShiftKind::kShiftOutZeros:
          kind = ShiftOp::Kind::kShiftRightArithmeticShiftOutZeros;
          break;
        case ShiftKind::kNormal:
          kind = ShiftOp::Kind::kShiftRightArithmetic;
          break;
      }
      return __ Shift(Map(node->InputAt(0)), Map(node->InputAt(1)), kind, rep);
    }

#define UNARY_CASE(opcode, assembler_op) \
  case IrOpcode::k##opcode:              \
    return __ assembler_op(Map(node->InputAt(0)));
#define TUPLE_UNARY_CASE(opcode, assembler_op)            \
  case IrOpcode::k##opcode: {                             \
    OpIndex idx = __ assembler_op(Map(node->InputAt(0))); \
    return EmitProjectionsAndTuple(idx);                  \
  }

      UNARY_CASE(Word32ReverseBytes, Word32ReverseBytes)
      UNARY_CASE(Word64ReverseBytes, Word64ReverseBytes)
      UNARY_CASE(Word32Clz, Word32CountLeadingZeros)
      UNARY_CASE(Word64Clz, Word64CountLeadingZeros)
      UNARY_CASE(Word32Ctz, Word32CountTrailingZeros)
      UNARY_CASE(Word64Ctz, Word64CountTrailingZeros)
      UNARY_CASE(Word32Popcnt, Word32PopCount)
      UNARY_CASE(Word64Popcnt, Word64PopCount)
      UNARY_CASE(SignExtendWord8ToInt32, Word32SignExtend8)
      UNARY_CASE(SignExtendWord16ToInt32, Word32SignExtend16)
      UNARY_CASE(SignExtendWord8ToInt64, Word64SignExtend8)
      UNARY_CASE(SignExtendWord16ToInt64, Word64SignExtend16)

      UNARY_CASE(Float32Abs, Float32Abs)
      UNARY_CASE(Float64Abs, Float64Abs)
      UNARY_CASE(Float32Neg, Float32Negate)
      UNARY_CASE(Float64Neg, Float64Negate)
      UNARY_CASE(Float64SilenceNaN, Float64SilenceNaN)
      UNARY_CASE(Float32RoundDown, Float32RoundDown)
      UNARY_CASE(Float64RoundDown, Float64RoundDown)
      UNARY_CASE(Float32RoundUp, Float32RoundUp)
      UNARY_CASE(Float64RoundUp, Float64RoundUp)
      UNARY_CASE(Float32RoundTruncate, Float32RoundToZero)
      UNARY_CASE(Float64RoundTruncate, Float64RoundToZero)
      UNARY_CASE(Float32RoundTiesEven, Float32RoundTiesEven)
      UNARY_CASE(Float64RoundTiesEven, Float64RoundTiesEven)
      UNARY_CASE(Float64Log, Float64Log)
      UNARY_CASE(Float32Sqrt, Float32Sqrt)
      UNARY_CASE(Float64Sqrt, Float64Sqrt)
      UNARY_CASE(Float64Exp, Float64Exp)
      UNARY_CASE(Float64Expm1, Float64Expm1)
      UNARY_CASE(Float64Sin, Float64Sin)
      UNARY_CASE(Float64Cos, Float64Cos)
      UNARY_CASE(Float64Sinh, Float64Sinh)
      UNARY_CASE(Float64Cosh, Float64Cosh)
      UNARY_CASE(Float64Asin, Float64Asin)
      UNARY_CASE(Float64Acos, Float64Acos)
      UNARY_CASE(Float64Asinh, Float64Asinh)
      UNARY_CASE(Float64Acosh, Float64Acosh)
      UNARY_CASE(Float64Tan, Float64Tan)
      UNARY_CASE(Float64Tanh, Float64Tanh)
      UNARY_CASE(Float64Log2, Float64Log2)
      UNARY_CASE(Float64Log10, Float64Log10)
      UNARY_CASE(Float64Log1p, Float64Log1p)
      UNARY_CASE(Float64Atan, Float64Atan)
      UNARY_CASE(Float64Atanh, Float64Atanh)
      UNARY_CASE(Float64Cbrt, Float64Cbrt)

      UNARY_CASE(BitcastWord32ToWord64, BitcastWord32ToWord64)
      UNARY_CASE(BitcastFloat32ToInt32, BitcastFloat32ToWord32)
      UNARY_CASE(BitcastInt32ToFloat32, BitcastWord32ToFloat32)
      UNARY_CASE(BitcastFloat64ToInt64, BitcastFloat64ToWord64)
      UNARY_CASE(BitcastInt64ToFloat64, BitcastWord64ToFloat64)
      UNARY_CASE(ChangeUint32ToUint64, ChangeUint32ToUint64)
      UNARY_CASE(ChangeInt32ToInt64, ChangeInt32ToInt64)
      UNARY_CASE(SignExtendWord32ToInt64, ChangeInt32ToInt64)

      UNARY_CASE(ChangeFloat32ToFloat64, ChangeFloat32ToFloat64)

      UNARY_CASE(ChangeFloat64ToInt32, ReversibleFloat64ToInt32)
      UNARY_CASE(ChangeFloat64ToInt64, ReversibleFloat64ToInt64)
      UNARY_CASE(ChangeFloat64ToUint32, ReversibleFloat64ToUint32)
      UNARY_CASE(ChangeFloat64ToUint64, ReversibleFloat64ToUint64)

      UNARY_CASE(ChangeInt32ToFloat64, ChangeInt32ToFloat64)
      UNARY_CASE(ChangeInt64ToFloat64, ReversibleInt64ToFloat64)
      UNARY_CASE(ChangeUint32ToFloat64, ChangeUint32ToFloat64)

      UNARY_CASE(RoundFloat64ToInt32, TruncateFloat64ToInt32OverflowUndefined)
      UNARY_CASE(RoundInt32ToFloat32, ChangeInt32ToFloat32)
      UNARY_CASE(RoundInt64ToFloat32, ChangeInt64ToFloat32)
      UNARY_CASE(RoundInt64ToFloat64, ChangeInt64ToFloat64)
      UNARY_CASE(RoundUint32ToFloat32, ChangeUint32ToFloat32)
      UNARY_CASE(RoundUint64ToFloat32, ChangeUint64ToFloat32)
      UNARY_CASE(RoundUint64ToFloat64, ChangeUint64ToFloat64)
      UNARY_CASE(TruncateFloat64ToFloat32, ChangeFloat64ToFloat32)
      UNARY_CASE(TruncateFloat64ToUint32,
                 TruncateFloat64ToUint32OverflowUndefined)
      UNARY_CASE(TruncateFloat64ToWord32, JSTruncateFloat64ToWord32)

      TUPLE_UNARY_CASE(TryTruncateFloat32ToInt64, TryTruncateFloat32ToInt64)
      TUPLE_UNARY_CASE(TryTruncateFloat32ToUint64, TryTruncateFloat32ToUint64)
      TUPLE_UNARY_CASE(TryTruncateFloat64ToInt32, TryTruncateFloat64ToInt32)
      TUPLE_UNARY_CASE(TryTruncateFloat64ToInt64, TryTruncateFloat64ToInt64)
      TUPLE_UNARY_CASE(TryTruncateFloat64ToUint32, TryTruncateFloat64ToUint32)
      TUPLE_UNARY_CASE(TryTruncateFloat64ToUint64, TryTruncateFloat64ToUint64)

      UNARY_CASE(Float64ExtractLowWord32, Float64ExtractLowWord32)
      UNARY_CASE(Float64ExtractHighWord32, Float64ExtractHighWord32)
#undef UNARY_CASE
#undef TUPLE_UNARY_CASE
    case IrOpcode::kTruncateInt64ToInt32:
      // 64- to 32-bit truncation is implicit in Turboshaft.
      return Map(node->InputAt(0));
    case IrOpcode::kTruncateFloat32ToInt32:
      switch (OpParameter<TruncateKind>(node->op())) {
        case TruncateKind::kArchitectureDefault:
          return __ TruncateFloat32ToInt32OverflowUndefined(
              Map(node->InputAt(0)));
        case TruncateKind::kSetOverflowToMin:
          return __ TruncateFloat32ToInt32OverflowToMin(Map(node->InputAt(0)));
      }
    case IrOpcode::kTruncateFloat32ToUint32:
      switch (OpParameter<TruncateKind>(node->op())) {
        case TruncateKind::kArchitectureDefault:
          return __ TruncateFloat32ToUint32OverflowUndefined(
              Map(node->InputAt(0)));
        case TruncateKind::kSetOverflowToMin:
          return __ TruncateFloat32ToUint32OverflowToMin(Map(node->InputAt(0)));
      }
    case IrOpcode::kTruncateFloat64ToInt64:
      switch (OpParameter<TruncateKind>(node->op())) {
        case TruncateKind::kArchitectureDefault:
          return __ TruncateFloat64ToInt64OverflowUndefined(
              Map(node->InputAt(0)));
        case TruncateKind::kSetOverflowToMin:
          return __ TruncateFloat64ToInt64OverflowToMin(Map(node->InputAt(0)));
      }
    case IrOpcode::kFloat64InsertLowWord32:
      return __ Float64InsertWord32(Map(node->InputAt(0)),
                                    Map(node->InputAt(1)),
                                    Float64InsertWord32Op::Kind::kLowHalf);
    case IrOpcode::kFloat64InsertHighWord32:
      return __ Float64InsertWord32(Map(node->InputAt(0)),
                                    Map(node->InputAt(1)),
                                    Float64InsertWord32Op::Kind::kHighHalf);

    case IrOpcode::kBitcastTaggedToWord:
      return __ TaggedBitcast(Map(node->InputAt(0)),
                              RegisterRepresentation::Tagged(),
                              RegisterRepresentation::PointerSized());
    case IrOpcode::kBitcastWordToTagged:
      return __ TaggedBitcast(Map(node->InputAt(0)),
                              RegisterRepresentation::PointerSized(),
                              RegisterRepresentation::Tagged());
    case IrOpcode::kNumberIsNaN:
      return __ FloatIs(Map(node->InputAt(0)), FloatIsOp::Kind::kNaN,
                        FloatRepresentation::Float64());

#define OBJECT_IS_CASE(kind)                                             \
  case IrOpcode::kObjectIs##kind: {                                      \
    return __ ObjectIs(Map(node->InputAt(0)), ObjectIsOp::Kind::k##kind, \
                       ObjectIsOp::InputAssumptions::kNone);             \
  }
      OBJECT_IS_CASE(ArrayBufferView)
      OBJECT_IS_CASE(BigInt)
      OBJECT_IS_CASE(Callable)
      OBJECT_IS_CASE(Constructor)
      OBJECT_IS_CASE(DetectableCallable)
      OBJECT_IS_CASE(NonCallable)
      OBJECT_IS_CASE(Number)
      OBJECT_IS_CASE(Receiver)
      OBJECT_IS_CASE(Smi)
      OBJECT_IS_CASE(String)
      OBJECT_IS_CASE(Symbol)
      OBJECT_IS_CASE(Undetectable)
#undef OBJECT_IS_CASE

#define CHECK_OBJECT_IS_CASE(code, kind, input_assumptions, reason, feedback) \
  case IrOpcode::k##code: {                                                   \
    DCHECK(dominating_frame_state.valid());                                   \
    V<Tagged> input = Map(node->InputAt(0));                                  \
    V<Word32> check =                                                         \
        __ ObjectIs(input, ObjectIsOp::Kind::k##kind,                         \
                    ObjectIsOp::InputAssumptions::k##input_assumptions);      \
    __ DeoptimizeIfNot(check, dominating_frame_state,                         \
                       DeoptimizeReason::k##reason, feedback);                \
    return input;                                                             \
  }
      CHECK_OBJECT_IS_CASE(CheckInternalizedString, InternalizedString,
                           HeapObject, WrongInstanceType, {})
      CHECK_OBJECT_IS_CASE(CheckNumber, Number, None, NotANumber,
                           CheckParametersOf(op).feedback())
      CHECK_OBJECT_IS_CASE(CheckReceiver, Receiver, HeapObject,
                           NotAJavaScriptObject, {})
      CHECK_OBJECT_IS_CASE(CheckReceiverOrNullOrUndefined,
                           ReceiverOrNullOrUndefined, HeapObject,
                           NotAJavaScriptObjectOrNullOrUndefined, {})
      CHECK_OBJECT_IS_CASE(CheckString, String, HeapObject, NotAString,
                           CheckParametersOf(op).feedback())
      CHECK_OBJECT_IS_CASE(CheckSymbol, Symbol, HeapObject, NotASymbol, {})
      CHECK_OBJECT_IS_CASE(CheckBigInt, BigInt, None, NotABigInt,
                           CheckParametersOf(op).feedback())
      CHECK_OBJECT_IS_CASE(CheckedBigIntToBigInt64, BigInt64, BigInt,
                           NotABigInt64, CheckParametersOf(op).feedback())
#undef CHECK_OBJECT_IS_CASE

#define CONVERT_TO_OBJECT_CASE(name, kind, input_type, input_interpretation) \
  case IrOpcode::k##name:                                                    \
    return __ ConvertToObject(                                               \
        Map(node->InputAt(0)), ConvertToObjectOp::Kind::k##kind,             \
        V<input_type>::rep,                                                  \
        ConvertToObjectOp::InputInterpretation::k##input_interpretation,     \
        CheckForMinusZeroMode::kDontCheckForMinusZero);
      CONVERT_TO_OBJECT_CASE(ChangeInt32ToTagged, Number, Word32, Signed)
      CONVERT_TO_OBJECT_CASE(ChangeUint32ToTagged, Number, Word32, Unsigned)
      CONVERT_TO_OBJECT_CASE(ChangeInt64ToTagged, Number, Word64, Signed)
      CONVERT_TO_OBJECT_CASE(ChangeUint64ToTagged, Number, Word64, Unsigned)
      CONVERT_TO_OBJECT_CASE(ChangeFloat64ToTaggedPointer, HeapNumber, Float64,
                             Signed)
      CONVERT_TO_OBJECT_CASE(ChangeInt64ToBigInt, BigInt, Word64, Signed)
      CONVERT_TO_OBJECT_CASE(ChangeUint64ToBigInt, BigInt, Word64, Unsigned)
      CONVERT_TO_OBJECT_CASE(ChangeInt31ToTaggedSigned, Smi, Word32, Signed)
      CONVERT_TO_OBJECT_CASE(ChangeBitToTagged, Boolean, Word32, Signed)
      CONVERT_TO_OBJECT_CASE(StringFromSingleCharCode, String, Word32, CharCode)
      CONVERT_TO_OBJECT_CASE(StringFromSingleCodePoint, String, Word32,
                             CodePoint)

    case IrOpcode::kChangeFloat64ToTagged:
      return __ ConvertToObject(Map(node->InputAt(0)),
                                ConvertToObjectOp::Kind::kNumber,
                                RegisterRepresentation::Float64(),
                                ConvertToObjectOp::InputInterpretation::kSigned,
                                CheckMinusZeroModeOf(node->op()));
#undef CONVERT_TO_OBJECT_CASE

#define CONVERT_TO_OBJECT_OR_DEOPT_CASE(name, kind, input_type,      \
                                        input_interpretation)        \
  case IrOpcode::k##name: {                                          \
    DCHECK(dominating_frame_state.valid());                          \
    const CheckParameters& params = CheckParametersOf(node->op());   \
    return __ ConvertToObjectOrDeopt(                                \
        Map(node->InputAt(0)), dominating_frame_state,               \
        ConvertToObjectOrDeoptOp::Kind::k##kind, V<input_type>::rep, \
        ConvertToObjectOrDeoptOp::InputInterpretation::              \
            k##input_interpretation,                                 \
        params.feedback());                                          \
  }
      CONVERT_TO_OBJECT_OR_DEOPT_CASE(CheckedInt32ToTaggedSigned, Smi, Word32,
                                      Signed)
      CONVERT_TO_OBJECT_OR_DEOPT_CASE(CheckedUint32ToTaggedSigned, Smi, Word32,
                                      Unsigned)
      CONVERT_TO_OBJECT_OR_DEOPT_CASE(CheckedInt64ToTaggedSigned, Smi, Word64,
                                      Signed)
      CONVERT_TO_OBJECT_OR_DEOPT_CASE(CheckedUint64ToTaggedSigned, Smi, Word64,
                                      Unsigned)
#undef CONVERT_TO_OBJECT_OR_DEOPT_CASE

#define CONVERT_OBJECT_TO_PRIMITIVE_CASE(name, kind, input_assumptions)   \
  case IrOpcode::k##name:                                                 \
    return __ ConvertObjectToPrimitive(                                   \
        Map(node->InputAt(0)), ConvertObjectToPrimitiveOp::Kind::k##kind, \
        ConvertObjectToPrimitiveOp::InputAssumptions::k##input_assumptions);
      CONVERT_OBJECT_TO_PRIMITIVE_CASE(ChangeTaggedSignedToInt32, Int32, Smi)
      CONVERT_OBJECT_TO_PRIMITIVE_CASE(ChangeTaggedSignedToInt64, Int64, Smi)
      CONVERT_OBJECT_TO_PRIMITIVE_CASE(ChangeTaggedToBit, Bit, Object)
      CONVERT_OBJECT_TO_PRIMITIVE_CASE(ChangeTaggedToInt32, Int32,
                                       NumberOrOddball)
      CONVERT_OBJECT_TO_PRIMITIVE_CASE(ChangeTaggedToUint32, Uint32,
                                       NumberOrOddball)
      CONVERT_OBJECT_TO_PRIMITIVE_CASE(ChangeTaggedToInt64, Int64,
                                       NumberOrOddball)
      CONVERT_OBJECT_TO_PRIMITIVE_CASE(ChangeTaggedToFloat64, Float64,
                                       NumberOrOddball)
      CONVERT_OBJECT_TO_PRIMITIVE_CASE(TruncateTaggedToFloat64, Float64,
                                       NumberOrOddball)
#undef CONVERT_OBJECT_TO_PRIMITIVE_CASE

#define TRUNCATE_OBJECT_TO_PRIMITIVE_CASE(name, kind, input_assumptions)   \
  case IrOpcode::k##name:                                                  \
    return __ TruncateObjectToPrimitive(                                   \
        Map(node->InputAt(0)), TruncateObjectToPrimitiveOp::Kind::k##kind, \
        TruncateObjectToPrimitiveOp::InputAssumptions::k##input_assumptions);
      TRUNCATE_OBJECT_TO_PRIMITIVE_CASE(TruncateTaggedToWord32, Int32,
                                        NumberOrOddball)
      TRUNCATE_OBJECT_TO_PRIMITIVE_CASE(TruncateBigIntToWord64, Int64, BigInt)
      TRUNCATE_OBJECT_TO_PRIMITIVE_CASE(TruncateTaggedToBit, Bit, Object)
      TRUNCATE_OBJECT_TO_PRIMITIVE_CASE(TruncateTaggedPointerToBit, Bit,
                                        HeapObject)
#undef TRUNCATE_OBJECT_TO_PRIMITIVE_CASE

#define CHANGE_OR_DEOPT_INT_CASE(kind)                                     \
  case IrOpcode::kChecked##kind: {                                         \
    DCHECK(dominating_frame_state.valid());                                \
    const CheckParameters& params = CheckParametersOf(node->op());         \
    return __ ChangeOrDeopt(Map(node->InputAt(0)), dominating_frame_state, \
                            ChangeOrDeoptOp::Kind::k##kind,                \
                            CheckForMinusZeroMode::kDontCheckForMinusZero, \
                            params.feedback());                            \
  }
      CHANGE_OR_DEOPT_INT_CASE(Uint32ToInt32)
      CHANGE_OR_DEOPT_INT_CASE(Int64ToInt32)
      CHANGE_OR_DEOPT_INT_CASE(Uint64ToInt32)
      CHANGE_OR_DEOPT_INT_CASE(Uint64ToInt64)
#undef CHANGE_OR_DEOPT_INT_CASE

    case IrOpcode::kCheckedFloat64ToInt32: {
      DCHECK(dominating_frame_state.valid());
      const CheckMinusZeroParameters& params =
          CheckMinusZeroParametersOf(node->op());
      return __ ChangeOrDeopt(Map(node->InputAt(0)), dominating_frame_state,
                              ChangeOrDeoptOp::Kind::kFloat64ToInt32,
                              params.mode(), params.feedback());
    }

    case IrOpcode::kCheckedFloat64ToInt64: {
      DCHECK(dominating_frame_state.valid());
      const CheckMinusZeroParameters& params =
          CheckMinusZeroParametersOf(node->op());
      return __ ChangeOrDeopt(Map(node->InputAt(0)), dominating_frame_state,
                              ChangeOrDeoptOp::Kind::kFloat64ToInt64,
                              params.mode(), params.feedback());
    }

    case IrOpcode::kCheckedTaggedToInt32: {
      DCHECK(dominating_frame_state.valid());
      const CheckMinusZeroParameters& params =
          CheckMinusZeroParametersOf(node->op());
      return __ ConvertObjectToPrimitiveOrDeopt(
          Map(node->InputAt(0)), dominating_frame_state,
          ConvertObjectToPrimitiveOrDeoptOp::ObjectKind::kNumber,
          ConvertObjectToPrimitiveOrDeoptOp::PrimitiveKind::kInt32,
          params.mode(), params.feedback());
    }

    case IrOpcode::kCheckedTaggedToInt64: {
      DCHECK(dominating_frame_state.valid());
      const CheckMinusZeroParameters& params =
          CheckMinusZeroParametersOf(node->op());
      return __ ConvertObjectToPrimitiveOrDeopt(
          Map(node->InputAt(0)), dominating_frame_state,
          ConvertObjectToPrimitiveOrDeoptOp::ObjectKind::kNumber,
          ConvertObjectToPrimitiveOrDeoptOp::PrimitiveKind::kInt64,
          params.mode(), params.feedback());
    }

    case IrOpcode::kCheckedTaggedToFloat64: {
      DCHECK(dominating_frame_state.valid());
      const CheckTaggedInputParameters& params =
          CheckTaggedInputParametersOf(node->op());
      ConvertObjectToPrimitiveOrDeoptOp::ObjectKind from_kind;
      switch (params.mode()) {
#define CASE(mode)                                                      \
  case CheckTaggedInputMode::k##mode:                                   \
    from_kind = ConvertObjectToPrimitiveOrDeoptOp::ObjectKind::k##mode; \
    break;
        CASE(Number)
        CASE(NumberOrBoolean)
        CASE(NumberOrOddball)
#undef CASE
      }
      return __ ConvertObjectToPrimitiveOrDeopt(
          Map(node->InputAt(0)), dominating_frame_state, from_kind,
          ConvertObjectToPrimitiveOrDeoptOp::PrimitiveKind::kFloat64,
          CheckForMinusZeroMode::kDontCheckForMinusZero, params.feedback());
    }

    case IrOpcode::kCheckedTaggedToArrayIndex: {
      DCHECK(dominating_frame_state.valid());
      const CheckParameters& params = CheckParametersOf(node->op());
      return __ ConvertObjectToPrimitiveOrDeopt(
          Map(node->InputAt(0)), dominating_frame_state,
          ConvertObjectToPrimitiveOrDeoptOp::ObjectKind::kNumberOrString,
          ConvertObjectToPrimitiveOrDeoptOp::PrimitiveKind::kArrayIndex,
          CheckForMinusZeroMode::kCheckForMinusZero, params.feedback());
    }

    case IrOpcode::kCheckedTaggedSignedToInt32: {
      DCHECK(dominating_frame_state.valid());
      const CheckParameters& params = CheckParametersOf(node->op());
      return __ ConvertObjectToPrimitiveOrDeopt(
          Map(node->InputAt(0)), dominating_frame_state,
          ConvertObjectToPrimitiveOrDeoptOp::ObjectKind::kSmi,
          ConvertObjectToPrimitiveOrDeoptOp::PrimitiveKind::kInt32,
          CheckForMinusZeroMode::kDontCheckForMinusZero, params.feedback());
    }

    case IrOpcode::kSelect: {
      OpIndex cond = Map(node->InputAt(0));
      OpIndex vtrue = Map(node->InputAt(1));
      OpIndex vfalse = Map(node->InputAt(2));
      const SelectParameters& params = SelectParametersOf(op);
      return __ Select(cond, vtrue, vfalse,
                       RegisterRepresentation::FromMachineRepresentation(
                           params.representation()),
                       params.hint(), SelectOp::Implementation::kBranch);
    }
    case IrOpcode::kWord32Select:
      return __ Select(Map(node->InputAt(0)), Map(node->InputAt(1)),
                       Map(node->InputAt(2)), RegisterRepresentation::Word32(),
                       BranchHint::kNone, SelectOp::Implementation::kCMove);
    case IrOpcode::kWord64Select:
      return __ Select(Map(node->InputAt(0)), Map(node->InputAt(1)),
                       Map(node->InputAt(2)), RegisterRepresentation::Word64(),
                       BranchHint::kNone, SelectOp::Implementation::kCMove);

    case IrOpcode::kLoad:
    case IrOpcode::kLoadImmutable:
    case IrOpcode::kUnalignedLoad: {
      MemoryRepresentation loaded_rep =
          MemoryRepresentation::FromMachineType(LoadRepresentationOf(op));
      Node* base = node->InputAt(0);
      Node* index = node->InputAt(1);
      // It's ok to merge LoadImmutable into Load after scheduling.
      LoadOp::Kind kind = opcode == IrOpcode::kUnalignedLoad
                              ? LoadOp::Kind::RawUnaligned()
                              : LoadOp::Kind::RawAligned();
      if (index->opcode() == IrOpcode::kInt32Constant) {
        int32_t offset = OpParameter<int32_t>(index->op());
        return __ Load(Map(base), kind, loaded_rep, offset);
      }
      if (index->opcode() == IrOpcode::kInt64Constant) {
        int64_t offset = OpParameter<int64_t>(index->op());
        if (base::IsValueInRangeForNumericType<int32_t>(offset)) {
          return __ Load(Map(base), kind, loaded_rep,
                         static_cast<int32_t>(offset));
        }
      }
      int32_t offset = 0;
      uint8_t element_size_log2 = 0;
      return __ Load(Map(base), Map(index), kind, loaded_rep, offset,
                     element_size_log2);
    }
    case IrOpcode::kProtectedLoad: {
      MemoryRepresentation loaded_rep =
          MemoryRepresentation::FromMachineType(LoadRepresentationOf(op));
      return __ Load(Map(node->InputAt(0)), Map(node->InputAt(1)),
                     LoadOp::Kind::Protected(), loaded_rep);
    }

    case IrOpcode::kStore:
    case IrOpcode::kUnalignedStore: {
      bool aligned = opcode != IrOpcode::kUnalignedStore;
      StoreRepresentation store_rep =
          aligned ? StoreRepresentationOf(op)
                  : StoreRepresentation(UnalignedStoreRepresentationOf(op),
                                        WriteBarrierKind::kNoWriteBarrier);
      StoreOp::Kind kind = opcode == IrOpcode::kStore
                               ? StoreOp::Kind::RawAligned()
                               : StoreOp::Kind::RawUnaligned();

      Node* base = node->InputAt(0);
      Node* index = node->InputAt(1);
      Node* value = node->InputAt(2);
      if (index->opcode() == IrOpcode::kInt32Constant) {
        int32_t offset = OpParameter<int32_t>(index->op());
        __ Store(Map(base), Map(value), kind,
                 MemoryRepresentation::FromMachineRepresentation(
                     store_rep.representation()),
                 store_rep.write_barrier_kind(), offset);
        return OpIndex::Invalid();
      }
      if (index->opcode() == IrOpcode::kInt64Constant) {
        int64_t offset = OpParameter<int64_t>(index->op());
        if (base::IsValueInRangeForNumericType<int32_t>(offset)) {
          __ Store(Map(base), Map(value), kind,
                   MemoryRepresentation::FromMachineRepresentation(
                       store_rep.representation()),
                   store_rep.write_barrier_kind(),
                   static_cast<int32_t>(offset));
          return OpIndex::Invalid();
        }
      }
      int32_t offset = 0;
      uint8_t element_size_log2 = 0;
      __ Store(Map(base), Map(index), Map(value), kind,
               MemoryRepresentation::FromMachineRepresentation(
                   store_rep.representation()),
               store_rep.write_barrier_kind(), offset, element_size_log2);
      return OpIndex::Invalid();
    }
    case IrOpcode::kProtectedStore:
      __ Store(Map(node->InputAt(0)), Map(node->InputAt(1)),
               Map(node->InputAt(2)), StoreOp::Kind::Protected(),
               MemoryRepresentation::FromMachineRepresentation(
                   OpParameter<MachineRepresentation>(node->op())),
               WriteBarrierKind::kNoWriteBarrier);
      return OpIndex::Invalid();

    case IrOpcode::kRetain:
      __ Retain(Map(node->InputAt(0)));
      return OpIndex::Invalid();
    case IrOpcode::kStackPointerGreaterThan:
      return __ StackPointerGreaterThan(Map(node->InputAt(0)),
                                        StackCheckKindOf(op));
    case IrOpcode::kLoadStackCheckOffset:
      return __ StackCheckOffset();
    case IrOpcode::kLoadFramePointer:
      return __ FramePointer();
    case IrOpcode::kLoadParentFramePointer:
      return __ ParentFramePointer();

    case IrOpcode::kStackSlot:
      return __ StackSlot(StackSlotRepresentationOf(op).size(),
                          StackSlotRepresentationOf(op).alignment());

    case IrOpcode::kBranch:
      DCHECK_EQ(block->SuccessorCount(), 2);
      __ Branch(Map(node->InputAt(0)), Map(block->SuccessorAt(0)),
                Map(block->SuccessorAt(1)), BranchHintOf(node->op()));
      return OpIndex::Invalid();

    case IrOpcode::kSwitch: {
      BasicBlock* default_branch = block->successors().back();
      DCHECK_EQ(IrOpcode::kIfDefault, default_branch->front()->opcode());
      size_t case_count = block->SuccessorCount() - 1;
      base::SmallVector<SwitchOp::Case, 16> cases;
      for (size_t i = 0; i < case_count; ++i) {
        BasicBlock* branch = block->SuccessorAt(i);
        const IfValueParameters& p = IfValueParametersOf(branch->front()->op());
        cases.emplace_back(p.value(), Map(branch), p.hint());
      }
      __ Switch(
          Map(node->InputAt(0)), graph_zone->CloneVector(base::VectorOf(cases)),
          Map(default_branch), BranchHintOf(default_branch->front()->op()));
      return OpIndex::Invalid();
    }

    case IrOpcode::kCall: {
      auto call_descriptor = CallDescriptorOf(op);
      base::SmallVector<OpIndex, 16> arguments;
      // The input `0` is the callee, the following value inputs are the
      // arguments. `CallDescriptor::InputCount()` counts the callee and
      // arguments, but excludes a possible `FrameState` input.
      OpIndex callee = Map(node->InputAt(0));
      for (int i = 1; i < static_cast<int>(call_descriptor->InputCount());
           ++i) {
        arguments.emplace_back(Map(node->InputAt(i)));
      }

      const TSCallDescriptor* ts_descriptor =
          TSCallDescriptor::Create(call_descriptor, graph_zone);

      OpIndex frame_state_idx = OpIndex::Invalid();
      if (call_descriptor->NeedsFrameState()) {
        FrameState frame_state{
            node->InputAt(static_cast<int>(call_descriptor->InputCount()))};
        frame_state_idx = Map(frame_state);
      }

      if (!is_final_control) {
        return EmitProjectionsAndTuple(__ Call(
            callee, frame_state_idx, base::VectorOf(arguments), ts_descriptor));
      } else {
        DCHECK_EQ(block->SuccessorCount(), 2);

        Block* if_success = Map(block->SuccessorAt(0));
        Block* if_exception = Map(block->SuccessorAt(1));
        // CallAndCatchException is a block terminator, so we can't generate the
        // projections right away. We'll generate them in the IfSuccess
        // successor.
        return __ CallAndCatchException(callee, frame_state_idx,
                                        base::VectorOf(arguments), if_success,
                                        if_exception, ts_descriptor);
      }
    }

    case IrOpcode::kTailCall: {
      auto call_descriptor = CallDescriptorOf(op);
      base::SmallVector<OpIndex, 16> arguments;
      // The input `0` is the callee, the following value inputs are the
      // arguments. `CallDescriptor::InputCount()` counts the callee and
      // arguments.
      OpIndex callee = Map(node->InputAt(0));
      for (int i = 1; i < static_cast<int>(call_descriptor->InputCount());
           ++i) {
        arguments.emplace_back(Map(node->InputAt(i)));
      }

      const TSCallDescriptor* ts_descriptor =
          TSCallDescriptor::Create(call_descriptor, graph_zone);

      __ TailCall(callee, base::VectorOf(arguments), ts_descriptor);
      return OpIndex::Invalid();
    }

    case IrOpcode::kFrameState: {
      FrameState frame_state{node};
      FrameStateData::Builder builder;
      BuildFrameStateData(&builder, frame_state);
      if (builder.Inputs().size() >
          std::numeric_limits<decltype(Operation::input_count)>::max() - 1) {
        *bailout = BailoutReason::kTooManyArguments;
        return OpIndex::Invalid();
      }
      return __ FrameState(builder.Inputs(), builder.inlined(),
                           builder.AllocateFrameStateData(
                               frame_state.frame_state_info(), graph_zone));
    }

    case IrOpcode::kDeoptimizeIf:
      __ DeoptimizeIf(Map(node->InputAt(0)), Map(node->InputAt(1)),
                      &DeoptimizeParametersOf(op));
      return OpIndex::Invalid();
    case IrOpcode::kDeoptimizeUnless:
      __ DeoptimizeIfNot(Map(node->InputAt(0)), Map(node->InputAt(1)),
                         &DeoptimizeParametersOf(op));
      return OpIndex::Invalid();

    case IrOpcode::kTrapIf:
      __ TrapIf(Map(node->InputAt(0)), TrapIdOf(op));
      return OpIndex::Invalid();
    case IrOpcode::kTrapUnless:
      __ TrapIfNot(Map(node->InputAt(0)), TrapIdOf(op));
      return OpIndex::Invalid();

    case IrOpcode::kDeoptimize: {
      OpIndex frame_state = Map(node->InputAt(0));
      __ Deoptimize(frame_state, &DeoptimizeParametersOf(op));
      return OpIndex::Invalid();
    }

    case IrOpcode::kReturn: {
      Node* pop_count = node->InputAt(0);
      base::SmallVector<OpIndex, 4> return_values;
      for (int i = 1; i < node->op()->ValueInputCount(); ++i) {
        return_values.push_back(Map(node->InputAt(i)));
      }
      __ Return(Map(pop_count), base::VectorOf(return_values));
      return OpIndex::Invalid();
    }
    case IrOpcode::kUnreachable:
    case IrOpcode::kThrow:
      __ Unreachable();
      return OpIndex::Invalid();

    case IrOpcode::kProjection: {
      Node* input = node->InputAt(0);
      size_t index = ProjectionIndexOf(op);
      RegisterRepresentation rep =
          RegisterRepresentation::FromMachineRepresentation(
              NodeProperties::GetProjectionType(node));
      return __ Projection(Map(input), index, rep);
    }

    case IrOpcode::kStaticAssert: {
      // We currently ignore StaticAsserts in turboshaft (because some of them
      // need specific unported optimizations to be evaluated).
      // TODO(turboshaft): once CommonOperatorReducer and MachineOperatorReducer
      // have been ported, re-enable StaticAsserts.
      // return __ ReduceStaticAssert(Map(node->InputAt(0)),
      //                                     StaticAssertSourceOf(node->op()));
      return OpIndex::Invalid();
    }

    case IrOpcode::kAllocate: {
      AllocationType allocation = AllocationTypeOf(node->op());
      return __ Allocate(Map(node->InputAt(0)), allocation,
                         AllowLargeObjects::kFalse);
    }
    // TODO(nicohartmann@): We might not see AllocateRaw here anymore.
    case IrOpcode::kAllocateRaw: {
      Node* size = node->InputAt(0);
      const AllocateParameters& params = AllocateParametersOf(node->op());
      return __ Allocate(Map(size), params.allocation_type(),
                         params.allow_large_objects());
    }
    case IrOpcode::kStoreToObject: {
      Node* object = node->InputAt(0);
      Node* offset = node->InputAt(1);
      Node* value = node->InputAt(2);
      ObjectAccess const& access = ObjectAccessOf(node->op());
      __ Store(Map(object), Map(offset), Map(value),
               StoreOp::Kind::TaggedBase(),
               MemoryRepresentation::FromMachineType(access.machine_type),
               access.write_barrier_kind, kHeapObjectTag);
      return OpIndex::Invalid();
    }
    case IrOpcode::kStoreElement: {
      Node* object = node->InputAt(0);
      Node* index = node->InputAt(1);
      Node* value = node->InputAt(2);
      ElementAccess const& access = ElementAccessOf(node->op());
      DCHECK(!access.machine_type.IsMapWord());
      StoreOp::Kind kind = StoreOp::Kind::Aligned(access.base_is_tagged);
      MemoryRepresentation rep =
          MemoryRepresentation::FromMachineType(access.machine_type);
      __ Store(Map(object), Map(index), Map(value), kind, rep,
               access.write_barrier_kind, access.header_size,
               rep.SizeInBytesLog2());
      return OpIndex::Invalid();
    }
    case IrOpcode::kStoreField: {
      OpIndex object = Map(node->InputAt(0));
      OpIndex value = Map(node->InputAt(1));
      FieldAccess const& access = FieldAccessOf(node->op());
      // External pointer must never be stored by optimized code.
      DCHECK(!access.type.Is(compiler::Type::ExternalPointer()) ||
             !V8_ENABLE_SANDBOX_BOOL);
      // SandboxedPointers are not currently stored by optimized code.
      DCHECK(!access.type.Is(compiler::Type::SandboxedPointer()));

#ifdef V8_ENABLE_SANDBOX
      if (access.is_bounded_size_access) {
        value = __ ShiftLeft(value, kBoundedSizeShift,
                             WordRepresentation::PointerSized());
      }
#endif  // V8_ENABLE_SANDBOX

      StoreOp::Kind kind = StoreOp::Kind::Aligned(access.base_is_tagged);
      MachineType machine_type = access.machine_type;
      if (machine_type.IsMapWord()) {
        machine_type = MachineType::TaggedPointer();
#ifdef V8_MAP_PACKING
        UNIMPLEMENTED();
#endif
      }
      MemoryRepresentation rep =
          MemoryRepresentation::FromMachineType(machine_type);
      __ Store(object, value, kind, rep, access.write_barrier_kind,
               access.offset);
      return OpIndex::Invalid();
    }
    case IrOpcode::kLoadFromObject:
    case IrOpcode::kLoadImmutableFromObject: {
      Node* object = node->InputAt(0);
      Node* offset = node->InputAt(1);
      ObjectAccess const& access = ObjectAccessOf(node->op());
      MemoryRepresentation rep =
          MemoryRepresentation::FromMachineType(access.machine_type);
      return __ Load(Map(object), Map(offset), LoadOp::Kind::TaggedBase(), rep,
                     kHeapObjectTag);
    }
    case IrOpcode::kLoadField: {
      Node* object = node->InputAt(0);
      FieldAccess const& access = FieldAccessOf(node->op());
      StoreOp::Kind kind = StoreOp::Kind::Aligned(access.base_is_tagged);
      MachineType machine_type = access.machine_type;
      if (machine_type.IsMapWord()) {
        machine_type = MachineType::TaggedPointer();
#ifdef V8_MAP_PACKING
        UNIMPLEMENTED();
#endif
      }
      MemoryRepresentation rep =
          MemoryRepresentation::FromMachineType(machine_type);
#ifdef V8_ENABLE_SANDBOX
      bool is_sandboxed_external =
          access.type.Is(compiler::Type::ExternalPointer());
      if (is_sandboxed_external) {
        // Fields for sandboxed external pointer contain a 32-bit handle, not a
        // 64-bit raw pointer.
        rep = MemoryRepresentation::Uint32();
      }
#endif  // V8_ENABLE_SANDBOX
      OpIndex value = __ Load(Map(object), kind, rep, access.offset);
#ifdef V8_ENABLE_SANDBOX
      if (is_sandboxed_external) {
        value = __ DecodeExternalPointer(value, access.external_pointer_tag);
      }
      if (access.is_bounded_size_access) {
        DCHECK(!is_sandboxed_external);
        value = __ ShiftRightLogical(value, kBoundedSizeShift,
                                     WordRepresentation::PointerSized());
      }
#endif  // V8_ENABLE_SANDBOX
      return value;
    }
    case IrOpcode::kLoadElement: {
      Node* object = node->InputAt(0);
      Node* index = node->InputAt(1);
      ElementAccess const& access = ElementAccessOf(node->op());
      LoadOp::Kind kind = LoadOp::Kind::Aligned(access.base_is_tagged);
      MemoryRepresentation rep =
          MemoryRepresentation::FromMachineType(access.machine_type);
      return __ Load(Map(object), Map(index), kind, rep, access.header_size,
                     rep.SizeInBytesLog2());
    }
    case IrOpcode::kCheckTurboshaftTypeOf: {
      Node* input = node->InputAt(0);
      Node* type_description = node->InputAt(1);

      HeapObjectMatcher m(type_description);
      CHECK(m.HasResolvedValue() && m.Ref(broker).IsString() &&
            m.Ref(broker).AsString().IsContentAccessible());
      StringRef type_string = m.Ref(broker).AsString();
      Handle<String> pattern_string =
          *type_string.ObjectIfContentAccessible(broker);
      std::unique_ptr<char[]> pattern = pattern_string->ToCString();

      auto type_opt =
          Type::ParseFromString(std::string_view{pattern.get()}, graph_zone);
      if (type_opt == base::nullopt) {
        FATAL(
            "String '%s' (of %d:CheckTurboshaftTypeOf) is not a valid type "
            "description!",
            pattern.get(), node->id());
      }

      OpIndex input_index = Map(input);
      RegisterRepresentation rep =
          __ output_graph().Get(input_index).outputs_rep()[0];
      return __ CheckTurboshaftTypeOf(input_index, rep, *type_opt, false);
    }

    case IrOpcode::kNewConsString:
      return __ NewConsString(Map(node->InputAt(0)), Map(node->InputAt(1)),
                              Map(node->InputAt(2)));
    case IrOpcode::kNewDoubleElements:
      return __ NewArray(Map(node->InputAt(0)), NewArrayOp::Kind::kDouble,
                         AllocationTypeOf(node->op()));
    case IrOpcode::kNewSmiOrObjectElements:
      return __ NewArray(Map(node->InputAt(0)), NewArrayOp::Kind::kObject,
                         AllocationTypeOf(node->op()));

    case IrOpcode::kDoubleArrayMin:
      return __ DoubleArrayMinMax(Map(node->InputAt(0)),
                                  DoubleArrayMinMaxOp::Kind::kMin);
    case IrOpcode::kDoubleArrayMax:
      return __ DoubleArrayMinMax(Map(node->InputAt(0)),
                                  DoubleArrayMinMaxOp::Kind::kMax);

    case IrOpcode::kLoadFieldByIndex:
      return __ LoadFieldByIndex(Map(node->InputAt(0)), Map(node->InputAt(1)));

    case IrOpcode::kCheckedInt64Add:
    case IrOpcode::kCheckedInt64Sub:
      DCHECK(Is64());
      [[fallthrough]];
    case IrOpcode::kCheckedInt32Add:
    case IrOpcode::kCheckedInt32Sub: {
      DCHECK(dominating_frame_state.valid());
      auto kind = (opcode == IrOpcode::kCheckedInt32Add ||
                   opcode == IrOpcode::kCheckedInt64Add)
                      ? OverflowCheckedBinopOp::Kind::kSignedAdd
                      : OverflowCheckedBinopOp::Kind::kSignedSub;
      auto rep = (opcode == IrOpcode::kCheckedInt32Add ||
                  opcode == IrOpcode::kCheckedInt32Sub)
                     ? WordRepresentation::Word32()
                     : WordRepresentation::Word64();

      OpIndex result = __ OverflowCheckedBinop(
          Map(node->InputAt(0)), Map(node->InputAt(1)), kind, rep);

      V<Word32> overflow =
          __ Projection(result, 1, RegisterRepresentation::Word32());
      __ DeoptimizeIf(overflow, dominating_frame_state,
                      DeoptimizeReason::kOverflow, FeedbackSource{});
      return __ Projection(result, 0, rep);
    }

    case IrOpcode::kCheckedInt32Mul: {
      DCHECK(dominating_frame_state.valid());
      V<Word32> lhs = Map(node->InputAt(0));
      V<Word32> rhs = Map(node->InputAt(1));

      CheckForMinusZeroMode mode = CheckMinusZeroModeOf(node->op());
      OpIndex result = __ Int32MulCheckOverflow(lhs, rhs);
      V<Word32> overflow =
          __ Projection(result, 1, RegisterRepresentation::Word32());
      __ DeoptimizeIf(overflow, dominating_frame_state,
                      DeoptimizeReason::kOverflow, FeedbackSource{});
      V<Word32> value =
          __ Projection(result, 0, RegisterRepresentation::Word32());

      if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
        IF(__ Word32Equal(value, 0)) {
          __ DeoptimizeIf(__ Int32LessThan(__ Word32BitwiseOr(lhs, rhs), 0),
                          dominating_frame_state, DeoptimizeReason::kMinusZero,
                          FeedbackSource{});
        }
        END_IF
      }

      return value;
    }

    case IrOpcode::kCheckedInt64Mul: {
      DCHECK(Is64());
      DCHECK(dominating_frame_state.valid());
      OpIndex result = __ Int64MulCheckOverflow(Map(node->InputAt(0)),
                                                Map(node->InputAt(1)));

      V<Word32> overflow =
          __ Projection(result, 1, RegisterRepresentation::Word32());
      __ DeoptimizeIf(overflow, dominating_frame_state,
                      DeoptimizeReason::kOverflow, FeedbackSource{});
      return __ Projection(result, 0, RegisterRepresentation::Word64());
    }

    case IrOpcode::kCheckedInt32Div: {
      DCHECK(dominating_frame_state.valid());
      V<Word32> lhs = Map(node->InputAt(0));
      V<Word32> rhs = Map(node->InputAt(1));

      // Check if the {rhs} is a known power of two.
      Int32Matcher m(node->InputAt(1));
      if (m.IsPowerOf2()) {
        // Since we know that {rhs} is a power of two, we can perform a fast
        // check to see if the relevant least significant bits of the {lhs}
        // are all zero, and if so we know that we can perform a division
        // safely (and fast by doing an arithmetic - aka sign preserving -
        // right shift on {lhs}).
        int32_t divisor = m.ResolvedValue();
        V<Word32> check =
            __ Word32Equal(__ Word32BitwiseAnd(lhs, divisor - 1), 0);
        __ DeoptimizeIfNot(check, dominating_frame_state,
                           DeoptimizeReason::kLostPrecision, FeedbackSource{});
        return __ Word32ShiftRightArithmeticShiftOutZeros(
            lhs, base::bits::WhichPowerOfTwo(divisor));
      } else {
        Label<Word32> done(this);

        // Check if {rhs} is positive (and not zero).
        IF(__ Int32LessThan(0, rhs)) { GOTO(done, __ Int32Div(lhs, rhs)); }
        ELSE {
          // Check if {rhs} is zero.
          __ DeoptimizeIf(__ Word32Equal(rhs, 0), dominating_frame_state,
                          DeoptimizeReason::kDivisionByZero, FeedbackSource{});

          // Check if {lhs} is zero, as that would produce minus zero.
          __ DeoptimizeIf(__ Word32Equal(lhs, 0), dominating_frame_state,
                          DeoptimizeReason::kMinusZero, FeedbackSource{});

          // Check if {lhs} is kMinInt and {rhs} is -1, in which case we'd have
          // to return -kMinInt, which is not representable as Word32.
          IF_UNLIKELY(__ Word32Equal(lhs, kMinInt)) {
            __ DeoptimizeIf(__ Word32Equal(rhs, -1), dominating_frame_state,
                            DeoptimizeReason::kOverflow, FeedbackSource{});
          }
          END_IF

          GOTO(done, __ Int32Div(lhs, rhs));
        }
        END_IF

        BIND(done, value);
        V<Word32> lossless = __ Word32Equal(lhs, __ Word32Mul(value, rhs));
        __ DeoptimizeIfNot(lossless, dominating_frame_state,
                           DeoptimizeReason::kLostPrecision, FeedbackSource{});
        return value;
      }
    }

    case IrOpcode::kCheckedInt64Div: {
      DCHECK(Is64());
      DCHECK(dominating_frame_state.valid());
      V<Word64> lhs = Map(node->InputAt(0));
      V<Word64> rhs = Map(node->InputAt(1));

      __ DeoptimizeIf(__ Word64Equal(rhs, 0), dominating_frame_state,
                      DeoptimizeReason::kDivisionByZero, FeedbackSource{});
      // Check if {lhs} is kMinInt64 and {rhs} is -1, in which case we'd have
      // to return -kMinInt64, which is not representable as Word64.
      IF_UNLIKELY(__ Word64Equal(lhs, std::numeric_limits<int64_t>::min())) {
        __ DeoptimizeIf(__ Word64Equal(rhs, int64_t{-1}),
                        dominating_frame_state, DeoptimizeReason::kOverflow,
                        FeedbackSource{});
      }
      END_IF

      return __ Int64Div(lhs, rhs);
    }

    case IrOpcode::kCheckedUint32Div: {
      DCHECK(dominating_frame_state.valid());
      V<Word32> lhs = Map(node->InputAt(0));
      V<Word32> rhs = Map(node->InputAt(1));

      // Check if the {rhs} is a known power of two.
      Uint32Matcher m(node->InputAt(1));
      if (m.IsPowerOf2()) {
        // Since we know that {rhs} is a power of two, we can perform a fast
        // check to see if the relevant least significant bits of the {lhs}
        // are all zero, and if so we know that we can perform a division
        // safely (and fast by doing a logical - aka zero extending - right
        // shift on {lhs}).
        uint32_t divisor = m.ResolvedValue();
        V<Word32> check =
            __ Word32Equal(__ Word32BitwiseAnd(lhs, divisor - 1), 0);
        __ DeoptimizeIfNot(check, dominating_frame_state,
                           DeoptimizeReason::kLostPrecision, FeedbackSource{});
        return __ Word32ShiftRightLogical(lhs,
                                          base::bits::WhichPowerOfTwo(divisor));
      } else {
        // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
        __ DeoptimizeIf(__ Word32Equal(rhs, 0), dominating_frame_state,
                        DeoptimizeReason::kDivisionByZero, FeedbackSource{});

        // Perform the actual unsigned integer division.
        V<Word32> value = __ Uint32Div(lhs, rhs);

        // Check if the remainder is non-zero.
        V<Word32> lossless = __ Word32Equal(lhs, __ Word32Mul(rhs, value));
        __ DeoptimizeIfNot(lossless, dominating_frame_state,
                           DeoptimizeReason::kLostPrecision, FeedbackSource{});
        return value;
      }
    }

    case IrOpcode::kCheckedInt32Mod: {
      DCHECK(dominating_frame_state.valid());
      // General case for signed integer modulus, with optimization for
      // (unknown) power of 2 right hand side.
      //
      //   if rhs <= 0 then
      //     rhs = -rhs
      //     deopt if rhs == 0
      //   if lhs < 0 then
      //     let lhs_abs = -lhs in
      //     let res = lhs_abs % rhs in
      //     deopt if res == 0
      //     -res
      //   else
      //     let msk = rhs - 1 in
      //     if rhs & msk == 0 then
      //       lhs & msk
      //     else
      //       lhs % rhs
      //
      V<Word32> lhs = Map(node->InputAt(0));
      V<Word32> rhs = Map(node->InputAt(1));

      Label<Word32> rhs_checked(this);
      Label<Word32> done(this);

      // Check if {rhs} is not strictly positive.
      IF(__ Int32LessThanOrEqual(rhs, 0)) {
        // Negate {rhs}, might still produce a negative result in case of
        // -2^31, but that is handled safely below.
        V<Word32> temp = __ Word32Sub(0, rhs);

        // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
        __ DeoptimizeIfNot(temp, dominating_frame_state,
                           DeoptimizeReason::kDivisionByZero, FeedbackSource{});
        GOTO(rhs_checked, temp);
      }
      ELSE { GOTO(rhs_checked, rhs); }
      END_IF

      BIND(rhs_checked, rhs_value);

      IF(__ Int32LessThan(lhs, 0)) {
        // The {lhs} is a negative integer. This is very unlikely and
        // we intentionally don't use the BuildUint32Mod() here, which
        // would try to figure out whether {rhs} is a power of two,
        // since this is intended to be a slow-path.
        V<Word32> temp = __ Uint32Mod(__ Word32Sub(0, lhs), rhs_value);

        // Check if we would have to return -0.
        __ DeoptimizeIf(__ Word32Equal(temp, 0), dominating_frame_state,
                        DeoptimizeReason::kMinusZero, FeedbackSource{});
        GOTO(done, __ Word32Sub(0, temp));
      }
      ELSE {
        // The {lhs} is a non-negative integer.
        GOTO(done, BuildUint32Mod(lhs, rhs_value));
      }
      END_IF

      BIND(done, result);
      return result;
    }

    case IrOpcode::kCheckedInt64Mod: {
      DCHECK(Is64());
      DCHECK(dominating_frame_state.valid());
      V<Word64> lhs = Map(node->InputAt(0));
      V<Word64> rhs = Map(node->InputAt(1));

      __ DeoptimizeIf(__ Word64Equal(rhs, 0), dominating_frame_state,
                      DeoptimizeReason::kDivisionByZero, FeedbackSource{});

      // While the mod-result cannot overflow, the underlying instruction is
      // `idiv` and will trap when the accompanying div-result overflows.
      IF_UNLIKELY(__ Word64Equal(lhs, std::numeric_limits<int64_t>::min())) {
        __ DeoptimizeIf(__ Word64Equal(rhs, int64_t{-1}),
                        dominating_frame_state, DeoptimizeReason::kOverflow,
                        FeedbackSource{});
      }
      END_IF

      return __ Int64Mod(lhs, rhs);
    }

    case IrOpcode::kCheckedUint32Mod: {
      DCHECK(dominating_frame_state.valid());
      V<Word32> lhs = Map(node->InputAt(0));
      V<Word32> rhs = Map(node->InputAt(1));

      // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
      __ DeoptimizeIf(__ Word32Equal(rhs, 0), dominating_frame_state,
                      DeoptimizeReason::kDivisionByZero, FeedbackSource{});

      // Perform the actual unsigned integer modulus.
      return BuildUint32Mod(lhs, rhs);
    }

#define BIGINT_BINOP_CASE(op, kind)                                     \
  case IrOpcode::kBigInt##op:                                           \
    DCHECK(dominating_frame_state.valid());                             \
    return __ BigIntBinop(Map(node->InputAt(0)), Map(node->InputAt(1)), \
                          dominating_frame_state,                       \
                          BigIntBinopOp::Kind::k##kind);
      BIGINT_BINOP_CASE(Add, Add)
      BIGINT_BINOP_CASE(Subtract, Sub)
      BIGINT_BINOP_CASE(Multiply, Mul)
      BIGINT_BINOP_CASE(Divide, Div)
      BIGINT_BINOP_CASE(Modulus, Mod)
      BIGINT_BINOP_CASE(BitwiseAnd, BitwiseAnd)
      BIGINT_BINOP_CASE(BitwiseOr, BitwiseOr)
      BIGINT_BINOP_CASE(BitwiseXor, BitwiseXor)
      BIGINT_BINOP_CASE(ShiftLeft, ShiftLeft)
      BIGINT_BINOP_CASE(ShiftRight, ShiftRightArithmetic)
#undef BIGINT_BINOP_CASE

    case IrOpcode::kBigIntEqual:
      return __ BigIntEqual(Map(node->InputAt(0)), Map(node->InputAt(1)));

    case IrOpcode::kBigIntLessThan:
      return __ BigIntLessThan(Map(node->InputAt(0)), Map(node->InputAt(1)));
    case IrOpcode::kBigIntLessThanOrEqual:
      return __ BigIntLessThanOrEqual(Map(node->InputAt(0)),
                                      Map(node->InputAt(1)));

    case IrOpcode::kBigIntNegate:
      return __ BigIntNegate(Map(node->InputAt(0)));

    case IrOpcode::kLoadRootRegister:
      // Inlined usage of wasm root register operation in JS.
      return assembler.ReduceLoadRootRegister();

    case IrOpcode::kStringCharCodeAt:
      return __ StringCharCodeAt(Map(node->InputAt(0)), Map(node->InputAt(1)));
    case IrOpcode::kStringCodePointAt:
      return __ StringCodePointAt(Map(node->InputAt(0)), Map(node->InputAt(1)));

#ifdef V8_INTL_SUPPORT
    case IrOpcode::kStringToLowerCaseIntl:
      return __ StringToLowerCaseIntl(Map(node->InputAt(0)));
    case IrOpcode::kStringToUpperCaseIntl:
      return __ StringToUpperCaseIntl(Map(node->InputAt(0)));
#else
    case IrOpcode::kStringToLowerCaseIntl:
    case IrOpcode::kStringToUpperCaseIntl:
      UNREACHABLE();
#endif  // V8_INTL_SUPPORT

    case IrOpcode::kStringLength:
      return __ StringLength(Map(node->InputAt(0)));

    case IrOpcode::kStringIndexOf:
      return __ StringIndexOf(Map(node->InputAt(0)), Map(node->InputAt(1)),
                              Map(node->InputAt(2)));

    case IrOpcode::kStringFromCodePointAt:
      return __ StringFromCodePointAt(Map(node->InputAt(0)),
                                      Map(node->InputAt(1)));

    case IrOpcode::kStringSubstring:
      return __ StringSubstring(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                Map(node->InputAt(2)));

    case IrOpcode::kStringEqual:
      return __ StringEqual(Map(node->InputAt(0)), Map(node->InputAt(1)));
    case IrOpcode::kStringLessThan:
      return __ StringLessThan(Map(node->InputAt(0)), Map(node->InputAt(1)));
    case IrOpcode::kStringLessThanOrEqual:
      return __ StringLessThanOrEqual(Map(node->InputAt(0)),
                                      Map(node->InputAt(1)));

    case IrOpcode::kBeginRegion:
      return OpIndex::Invalid();
    case IrOpcode::kFinishRegion:
      return Map(node->InputAt(0));

    default:
      std::cerr << "unsupported node type: " << *node->op() << "\n";
      node->Print(std::cerr);
      UNIMPLEMENTED();
  }
}

}  // namespace

base::Optional<BailoutReason> BuildGraph(JSHeapBroker* broker,
                                         Schedule* schedule, Isolate* isolate,
                                         Zone* graph_zone, Zone* phase_zone,
                                         Graph* graph, Linkage* linkage,
                                         SourcePositionTable* source_positions,
                                         NodeOriginTable* origins) {
  GraphBuilder builder{isolate,
                       broker,
                       graph_zone,
                       phase_zone,
                       *schedule,
                       Assembler<reducer_list<>>(*graph, *graph, phase_zone,
                                                 nullptr, std::tuple<>{}),
                       linkage,
                       source_positions,
                       origins};
  return builder.Run();
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
