// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/graph-builder.h"

#include <limits>
#include <numeric>

#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/safe_conversions.h"
#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/state-values-utils.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/deopt-data.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operations.h"

namespace v8::internal::compiler::turboshaft {

namespace {

struct GraphBuilder {
  Zone* graph_zone;
  Zone* phase_zone;
  Schedule& schedule;
  Assembler assembler;
  SourcePositionTable* source_positions;
  NodeOriginTable* origins;

  NodeAuxData<OpIndex> op_mapping{phase_zone};
  ZoneVector<Block*> block_mapping{schedule.RpoBlockCount(), phase_zone};

  base::Optional<BailoutReason> Run();

 private:
  OpIndex Map(Node* old_node) {
    OpIndex result = op_mapping.Get(old_node);
    DCHECK(assembler.graph().IsValid(result));
    return result;
  }
  Block* Map(BasicBlock* block) {
    Block* result = block_mapping[block->rpo_number()];
    DCHECK_NOT_NULL(result);
    return result;
  }

  void FixLoopPhis(Block* loop, Block* backedge) {
    DCHECK(loop->IsLoop());
    for (Operation& op : assembler.graph().operations(*loop)) {
      if (!op.Is<PendingLoopPhiOp>()) continue;
      auto& pending_phi = op.Cast<PendingLoopPhiOp>();
      assembler.graph().Replace<PhiOp>(
          assembler.graph().Index(pending_phi),
          base::VectorOf(
              {pending_phi.first(), Map(pending_phi.old_backedge_node)}),
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
  OpIndex Process(Node* node, BasicBlock* block,
                  const base::SmallVector<int, 16>& predecessor_permutation);
};

base::Optional<BailoutReason> GraphBuilder::Run() {
  for (BasicBlock* block : *schedule.rpo_order()) {
    block_mapping[block->rpo_number()] = assembler.NewBlock(BlockKind(block));
  }
  for (BasicBlock* block : *schedule.rpo_order()) {
    Block* target_block = Map(block);
    if (!assembler.Bind(target_block)) continue;
    target_block->SetDeferred(block->deferred());

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

    for (Node* node : *block->nodes()) {
      if (V8_UNLIKELY(node->InputCount() >=
                      int{std::numeric_limits<
                          decltype(Operation::input_count)>::max()})) {
        return BailoutReason::kTooManyArguments;
      }
      OpIndex i = Process(node, block, predecessor_permutation);
      op_mapping.Set(node, i);
    }
    if (Node* node = block->control_input()) {
      if (V8_UNLIKELY(node->InputCount() >=
                      int{std::numeric_limits<
                          decltype(Operation::input_count)>::max()})) {
        return BailoutReason::kTooManyArguments;
      }
      OpIndex i = Process(node, block, predecessor_permutation);
      op_mapping.Set(node, i);
    }
    switch (block->control()) {
      case BasicBlock::kGoto: {
        DCHECK_EQ(block->SuccessorCount(), 1);
        Block* destination = Map(block->SuccessorAt(0));
        assembler.Goto(destination);
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
        break;
      case BasicBlock::kCall: {
        Node* call = block->control_input();
        DCHECK_EQ(call->opcode(), IrOpcode::kCall);
        DCHECK_EQ(block->SuccessorCount(), 2);
        Block* if_success = Map(block->SuccessorAt(0));
        Block* if_exception = Map(block->SuccessorAt(1));
        OpIndex catch_exception =
            assembler.CatchException(Map(call), if_success, if_exception);
        Node* if_exception_node = block->SuccessorAt(1)->NodeAt(0);
        DCHECK_EQ(if_exception_node->opcode(), IrOpcode::kIfException);
        op_mapping.Set(if_exception_node, catch_exception);
        break;
      }
      case BasicBlock::kTailCall:
        UNIMPLEMENTED();
      case BasicBlock::kNone:
        UNREACHABLE();
    }
    DCHECK_NULL(assembler.current_block());
  }

  if (source_positions->IsEnabled()) {
    for (OpIndex index : assembler.graph().AllOperationIndices()) {
      compiler::NodeId origin =
          assembler.graph().operation_origins()[index].DecodeTurbofanNodeId();
      assembler.graph().source_positions()[index] =
          source_positions->GetSourcePosition(origin);
    }
  }

  if (origins) {
    for (OpIndex index : assembler.graph().AllOperationIndices()) {
      OpIndex origin = assembler.graph().operation_origins()[index];
      origins->SetNodeOrigin(index.id(), origin.DecodeTurbofanNodeId());
    }
  }

  return base::nullopt;
}

OpIndex GraphBuilder::Process(
    Node* node, BasicBlock* block,
    const base::SmallVector<int, 16>& predecessor_permutation) {
  assembler.SetCurrentOrigin(OpIndex::EncodeTurbofanNodeId(node->id()));
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
    case IrOpcode::kIfSuccess:
      return OpIndex::Invalid();

    case IrOpcode::kIfException: {
      // Use the `CatchExceptionOp` that has already been produced when
      // processing the call.
      OpIndex catch_exception = Map(node);
      DCHECK(assembler.graph().Get(catch_exception).Is<CatchExceptionOp>());
      return catch_exception;
    }

    case IrOpcode::kParameter: {
      const ParameterInfo& info = ParameterInfoOf(op);
      return assembler.Parameter(info.index(), info.debug_name());
    }

    case IrOpcode::kOsrValue: {
      return assembler.OsrValue(OsrValueIndexOf(op));
    }

    case IrOpcode::kPhi: {
      int input_count = op->ValueInputCount();
      MachineRepresentation rep = PhiRepresentationOf(op);
      if (assembler.current_block()->IsLoop()) {
        DCHECK_EQ(input_count, 2);
        return assembler.PendingLoopPhi(Map(node->InputAt(0)), rep,
                                        node->InputAt(1));
      } else {
        base::SmallVector<OpIndex, 16> inputs;
        for (int i = 0; i < input_count; ++i) {
          inputs.push_back(Map(node->InputAt(predecessor_permutation[i])));
        }
        return assembler.Phi(base::VectorOf(inputs), rep);
      }
    }

    case IrOpcode::kInt64Constant:
      return assembler.Word64Constant(
          static_cast<uint64_t>(OpParameter<int64_t>(op)));
    case IrOpcode::kInt32Constant:
      return assembler.Word32Constant(
          static_cast<uint32_t>(OpParameter<int32_t>(op)));
    case IrOpcode::kFloat64Constant:
      return assembler.Float64Constant(OpParameter<double>(op));
    case IrOpcode::kFloat32Constant:
      return assembler.Float32Constant(OpParameter<float>(op));
    case IrOpcode::kNumberConstant:
      return assembler.NumberConstant(OpParameter<double>(op));
    case IrOpcode::kTaggedIndexConstant:
      return assembler.TaggedIndexConstant(OpParameter<int32_t>(op));
    case IrOpcode::kHeapConstant:
      return assembler.HeapConstant(HeapConstantOf(op));
    case IrOpcode::kCompressedHeapConstant:
      return assembler.CompressedHeapConstant(HeapConstantOf(op));
    case IrOpcode::kExternalConstant:
      return assembler.ExternalConstant(OpParameter<ExternalReference>(op));

#define BINOP_CASE(opcode, assembler_op) \
  case IrOpcode::k##opcode:              \
    return assembler.assembler_op(Map(node->InputAt(0)), Map(node->InputAt(1)));

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
      BINOP_CASE(Uint32MulHigh, Uint32MulOverflownBits)

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

      BINOP_CASE(Int32AddWithOverflow, Int32AddCheckOverflow)
      BINOP_CASE(Int64AddWithOverflow, Int64AddCheckOverflow)
      BINOP_CASE(Int32MulWithOverflow, Int32MulCheckOverflow)
      BINOP_CASE(Int32SubWithOverflow, Int32SubCheckOverflow)
      BINOP_CASE(Int64SubWithOverflow, Int64SubCheckOverflow)

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

    case IrOpcode::kWord64Sar:
    case IrOpcode::kWord32Sar: {
      MachineRepresentation rep = opcode == IrOpcode::kWord64Sar
                                      ? MachineRepresentation::kWord64
                                      : MachineRepresentation::kWord32;
      ShiftOp::Kind kind;
      switch (ShiftKindOf(op)) {
        case ShiftKind::kShiftOutZeros:
          kind = ShiftOp::Kind::kShiftRightArithmeticShiftOutZeros;
          break;
        case ShiftKind::kNormal:
          kind = ShiftOp::Kind::kShiftRightArithmetic;
          break;
      }
      return assembler.Shift(Map(node->InputAt(0)), Map(node->InputAt(1)), kind,
                             rep);
    }

#define UNARY_CASE(opcode, assembler_op) \
  case IrOpcode::k##opcode:              \
    return assembler.assembler_op(Map(node->InputAt(0)));

      UNARY_CASE(Word32ReverseBytes, Word32ReverseBytes)
      UNARY_CASE(Word64ReverseBytes, Word64ReverseBytes)
      UNARY_CASE(Word32Clz, Word32CountLeadingZeros)
      UNARY_CASE(Word64Clz, Word64CountLeadingZeros)

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
#undef UNARY_CASE

#define CHANGE_CASE(opcode, kind, from, to)                                 \
  case IrOpcode::k##opcode:                                                 \
    return assembler.Change(Map(node->InputAt(0)), ChangeOp::Kind::k##kind, \
                            MachineRepresentation::k##from,                 \
                            MachineRepresentation::k##to);

      CHANGE_CASE(BitcastWord32ToWord64, Bitcast, Word32, Word64)
      CHANGE_CASE(BitcastFloat32ToInt32, Bitcast, Float32, Word32)
      CHANGE_CASE(BitcastInt32ToFloat32, Bitcast, Word32, Float32)
      CHANGE_CASE(BitcastFloat64ToInt64, Bitcast, Float64, Word64)
      CHANGE_CASE(BitcastInt64ToFloat64, Bitcast, Word64, Float64)
      CHANGE_CASE(ChangeUint32ToUint64, ZeroExtend, Word32, Word64)
      CHANGE_CASE(ChangeInt32ToInt64, SignExtend, Word32, Word64)
      CHANGE_CASE(ChangeInt32ToFloat64, SignedToFloat, Word32, Float64)
      CHANGE_CASE(ChangeInt64ToFloat64, SignedToFloat, Word64, Float64)
      CHANGE_CASE(ChangeUint32ToFloat64, UnsignedToFloat, Word32, Float64)
      CHANGE_CASE(TruncateFloat64ToWord32, JSFloatTruncate, Float64, Word32)
      CHANGE_CASE(TruncateFloat64ToFloat32, FloatConversion, Float64, Float32)
      CHANGE_CASE(ChangeFloat32ToFloat64, FloatConversion, Float32, Float64)
      CHANGE_CASE(RoundFloat64ToInt32, SignedFloatTruncate, Float64, Word32)
      CHANGE_CASE(ChangeFloat64ToInt32, SignedNarrowing, Float64, Word32)
      CHANGE_CASE(ChangeFloat64ToUint32, UnsignedNarrowing, Float64, Word32)
      CHANGE_CASE(ChangeFloat64ToInt64, SignedNarrowing, Float64, Word64)
      CHANGE_CASE(ChangeFloat64ToUint64, UnsignedNarrowing, Float64, Word64)
      CHANGE_CASE(Float64ExtractLowWord32, ExtractLowHalf, Float64, Word32)
      CHANGE_CASE(Float64ExtractHighWord32, ExtractHighHalf, Float64, Word32)
#undef CHANGE_CASE
    case IrOpcode::kTruncateInt64ToInt32:
      // 64- to 32-bit truncation is implicit in Turboshaft.
      return Map(node->InputAt(0));
    case IrOpcode::kTruncateFloat64ToInt64: {
      ChangeOp::Kind kind;
      switch (OpParameter<TruncateKind>(op)) {
        case TruncateKind::kArchitectureDefault:
          kind = ChangeOp::Kind::kSignedFloatTruncate;
          break;
        case TruncateKind::kSetOverflowToMin:
          kind = ChangeOp::Kind::kSignedFloatTruncateOverflowToMin;
          break;
      }
      return assembler.Change(Map(node->InputAt(0)), kind,
                              MachineRepresentation::kFloat64,
                              MachineRepresentation::kWord64);
    }

    case IrOpcode::kFloat64InsertLowWord32:
      return assembler.Float64InsertWord32(
          Map(node->InputAt(0)), Map(node->InputAt(1)),
          Float64InsertWord32Op::Kind::kLowHalf);
    case IrOpcode::kFloat64InsertHighWord32:
      return assembler.Float64InsertWord32(
          Map(node->InputAt(0)), Map(node->InputAt(1)),
          Float64InsertWord32Op::Kind::kHighHalf);

    case IrOpcode::kBitcastTaggedToWord:
      return assembler.TaggedBitcast(Map(node->InputAt(0)),
                                     MachineRepresentation::kTagged,
                                     MachineType::PointerRepresentation());
    case IrOpcode::kBitcastWordToTagged:
      return assembler.TaggedBitcast(Map(node->InputAt(0)),
                                     MachineType::PointerRepresentation(),
                                     MachineRepresentation::kTagged);

    case IrOpcode::kLoad:
    case IrOpcode::kUnalignedLoad: {
      MachineType loaded_rep = LoadRepresentationOf(op);
      Node* base = node->InputAt(0);
      Node* index = node->InputAt(1);
      LoadOp::Kind kind = opcode == IrOpcode::kLoad
                              ? LoadOp::Kind::kRawAligned
                              : LoadOp::Kind::kRawUnaligned;
      if (index->opcode() == IrOpcode::kInt32Constant) {
        int32_t offset = OpParameter<int32_t>(index->op());
        return assembler.Load(Map(base), kind, loaded_rep, offset);
      }
      if (index->opcode() == IrOpcode::kInt64Constant) {
        int64_t offset = OpParameter<int64_t>(index->op());
        if (base::IsValueInRangeForNumericType<int32_t>(offset)) {
          return assembler.Load(Map(base), kind, loaded_rep,
                                static_cast<int32_t>(offset));
        }
      }
      int32_t offset = 0;
      uint8_t element_size_log2 = 0;
      return assembler.IndexedLoad(Map(base), Map(index), kind, loaded_rep,
                                   offset, element_size_log2);
    }

    case IrOpcode::kStore:
    case IrOpcode::kUnalignedStore: {
      bool aligned = opcode == IrOpcode::kStore;
      StoreRepresentation store_rep =
          aligned ? StoreRepresentationOf(op)
                  : StoreRepresentation(UnalignedStoreRepresentationOf(op),
                                        WriteBarrierKind::kNoWriteBarrier);
      StoreOp::Kind kind =
          aligned ? StoreOp::Kind::kRawAligned : StoreOp::Kind::kRawUnaligned;
      Node* base = node->InputAt(0);
      Node* index = node->InputAt(1);
      Node* value = node->InputAt(2);
      if (index->opcode() == IrOpcode::kInt32Constant) {
        int32_t offset = OpParameter<int32_t>(index->op());
        return assembler.Store(Map(base), Map(value), kind,
                               store_rep.representation(),
                               store_rep.write_barrier_kind(), offset);
      }
      if (index->opcode() == IrOpcode::kInt64Constant) {
        int64_t offset = OpParameter<int64_t>(index->op());
        if (base::IsValueInRangeForNumericType<int32_t>(offset)) {
          return assembler.Store(
              Map(base), Map(value), kind, store_rep.representation(),
              store_rep.write_barrier_kind(), static_cast<int32_t>(offset));
        }
      }
      int32_t offset = 0;
      uint8_t element_size_log2 = 0;
      return assembler.IndexedStore(
          Map(base), Map(index), Map(value), kind, store_rep.representation(),
          store_rep.write_barrier_kind(), offset, element_size_log2);
    }

    case IrOpcode::kRetain:
      return assembler.Retain(Map(node->InputAt(0)));

    case IrOpcode::kStackPointerGreaterThan:
      return assembler.StackPointerGreaterThan(Map(node->InputAt(0)),
                                               StackCheckKindOf(op));
    case IrOpcode::kLoadStackCheckOffset:
      return assembler.FrameConstant(FrameConstantOp::Kind::kStackCheckOffset);
    case IrOpcode::kLoadFramePointer:
      return assembler.FrameConstant(FrameConstantOp::Kind::kFramePointer);
    case IrOpcode::kLoadParentFramePointer:
      return assembler.FrameConstant(
          FrameConstantOp::Kind::kParentFramePointer);

    case IrOpcode::kStackSlot:
      return assembler.StackSlot(StackSlotRepresentationOf(op).size(),
                                 StackSlotRepresentationOf(op).alignment());

    case IrOpcode::kBranch:
      DCHECK_EQ(block->SuccessorCount(), 2);
      return assembler.Branch(Map(node->InputAt(0)), Map(block->SuccessorAt(0)),
                              Map(block->SuccessorAt(1)));

    case IrOpcode::kSwitch: {
      BasicBlock* default_branch = block->successors().back();
      DCHECK_EQ(IrOpcode::kIfDefault, default_branch->front()->opcode());
      size_t case_count = block->SuccessorCount() - 1;
      base::SmallVector<SwitchOp::Case, 16> cases;
      for (size_t i = 0; i < case_count; ++i) {
        BasicBlock* branch = block->SuccessorAt(i);
        const IfValueParameters& p = IfValueParametersOf(branch->front()->op());
        cases.emplace_back(p.value(), Map(branch));
      }
      return assembler.Switch(Map(node->InputAt(0)),
                              graph_zone->CloneVector(base::VectorOf(cases)),
                              Map(default_branch));
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
      OpIndex call =
          assembler.Call(callee, base::VectorOf(arguments), call_descriptor);
      if (!call_descriptor->NeedsFrameState()) return call;
      FrameState frame_state{
          node->InputAt(static_cast<int>(call_descriptor->InputCount()))};
      assembler.CheckLazyDeopt(call, Map(frame_state));
      return call;
    }

    case IrOpcode::kFrameState: {
      FrameState frame_state{node};
      FrameStateData::Builder builder;
      BuildFrameStateData(&builder, frame_state);
      return assembler.FrameState(
          builder.Inputs(), builder.inlined(),
          builder.AllocateFrameStateData(frame_state.frame_state_info(),
                                         graph_zone));
    }

    case IrOpcode::kDeoptimizeIf:
    case IrOpcode::kDeoptimizeUnless: {
      OpIndex condition = Map(node->InputAt(0));
      OpIndex frame_state = Map(node->InputAt(1));
      bool negated = op->opcode() == IrOpcode::kDeoptimizeUnless;
      return assembler.DeoptimizeIf(condition, frame_state, negated,
                                    &DeoptimizeParametersOf(op));
    }

    case IrOpcode::kDeoptimize: {
      OpIndex frame_state = Map(node->InputAt(0));
      return assembler.Deoptimize(frame_state, &DeoptimizeParametersOf(op));
    }

    case IrOpcode::kReturn: {
      Node* pop_count = node->InputAt(0);
      base::SmallVector<OpIndex, 4> return_values;
      for (int i = 1; i < node->op()->ValueInputCount(); ++i) {
        return_values.push_back(Map(node->InputAt(i)));
      }
      return assembler.Return(Map(pop_count), base::VectorOf(return_values));
    }

    case IrOpcode::kUnreachable:
      for (Node* use : node->uses()) {
        CHECK_EQ(use->opcode(), IrOpcode::kThrow);
      }
      return OpIndex::Invalid();
    case IrOpcode::kThrow:
      return assembler.Unreachable();

    case IrOpcode::kProjection: {
      Node* input = node->InputAt(0);
      size_t index = ProjectionIndexOf(op);
      return assembler.Projection(Map(input), index);
    }

    default:
      std::cout << "unsupported node type: " << *node->op() << "\n";
      node->Print();
      UNIMPLEMENTED();
  }
}

}  // namespace

base::Optional<BailoutReason> BuildGraph(Schedule* schedule, Zone* graph_zone,
                                         Zone* phase_zone, Graph* graph,
                                         SourcePositionTable* source_positions,
                                         NodeOriginTable* origins) {
  GraphBuilder builder{graph_zone,       phase_zone,
                       *schedule,        Assembler(graph, phase_zone),
                       source_positions, origins};
  return builder.Run();
}

}  // namespace v8::internal::compiler::turboshaft
