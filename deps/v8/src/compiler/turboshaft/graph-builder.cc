// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/graph-builder.h"

#include <limits>
#include <numeric>

#include "src/base/logging.h"
#include "src/base/safe_conversions.h"
#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-aux-data.h"
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
  NodeAuxData<OpIndex> op_mapping{phase_zone};
  ZoneVector<Block*> block_mapping{schedule.RpoBlockCount(), phase_zone};

  void Run();

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

void GraphBuilder::Run() {
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
      OpIndex i = Process(node, block, predecessor_permutation);
      op_mapping.Set(node, i);
    }
    if (Node* node = block->control_input()) {
      OpIndex i = Process(node, block, predecessor_permutation);
      op_mapping.Set(node, i);
    }
    switch (block->control()) {
      case BasicBlock::kGoto: {
        DCHECK_EQ(block->SuccessorCount(), 1);
        Block* destination = Map(block->SuccessorAt(0));
        assembler.Goto(destination);
        if (destination->IsLoop()) {
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
      case BasicBlock::kCall:
      case BasicBlock::kTailCall:
        UNIMPLEMENTED();
      case BasicBlock::kNone:
        UNREACHABLE();
    }
    DCHECK_NULL(assembler.current_block());
  }
}

OpIndex GraphBuilder::Process(
    Node* node, BasicBlock* block,
    const base::SmallVector<int, 16>& predecessor_permutation) {
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
    case IrOpcode::kTypedStateValues:
    case IrOpcode::kObjectId:
    case IrOpcode::kTypedObjectState:
    case IrOpcode::kEffectPhi:
    case IrOpcode::kTerminate:
      return OpIndex::Invalid();

    case IrOpcode::kParameter: {
      const ParameterInfo& info = ParameterInfoOf(op);
      return assembler.Parameter(info.index(), info.debug_name());
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
      return assembler.Constant(
          ConstantOp::Kind::kWord64,
          static_cast<uint64_t>(OpParameter<int64_t>(op)));
    case IrOpcode::kInt32Constant:
      return assembler.Constant(
          ConstantOp::Kind::kWord32,
          uint64_t{static_cast<uint32_t>(OpParameter<int32_t>(op))});
    case IrOpcode::kFloat64Constant:
      return assembler.Constant(ConstantOp::Kind::kFloat64,
                                OpParameter<double>(op));
    case IrOpcode::kFloat32Constant:
      return assembler.Constant(ConstantOp::Kind::kFloat32,
                                OpParameter<float>(op));
    case IrOpcode::kNumberConstant:
      return assembler.Constant(ConstantOp::Kind::kNumber,
                                OpParameter<double>(op));
    case IrOpcode::kTaggedIndexConstant:
      return assembler.Constant(
          ConstantOp::Kind::kTaggedIndex,
          uint64_t{static_cast<uint32_t>(OpParameter<int32_t>(op))});
    case IrOpcode::kHeapConstant:
      return assembler.Constant(ConstantOp::Kind::kHeapObject,
                                HeapConstantOf(op));
    case IrOpcode::kCompressedHeapConstant:
      return assembler.Constant(ConstantOp::Kind::kCompressedHeapObject,
                                HeapConstantOf(op));
    case IrOpcode::kExternalConstant:
      return assembler.Constant(ConstantOp::Kind::kExternal,
                                OpParameter<ExternalReference>(op));
    case IrOpcode::kDelayedStringConstant:
      return assembler.Constant(ConstantOp::Kind::kDelayedString,
                                StringConstantBaseOf(op));

    case IrOpcode::kWord32And:
      return assembler.BitwiseAnd(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  MachineRepresentation::kWord32);
    case IrOpcode::kWord64And:
      return assembler.BitwiseAnd(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  MachineRepresentation::kWord64);

    case IrOpcode::kWord32Or:
      return assembler.BitwiseOr(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                 MachineRepresentation::kWord32);
    case IrOpcode::kWord64Or:
      return assembler.BitwiseOr(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                 MachineRepresentation::kWord64);

    case IrOpcode::kWord32Xor:
      return assembler.BitwiseXor(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  MachineRepresentation::kWord32);
    case IrOpcode::kWord64Xor:
      return assembler.BitwiseXor(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  MachineRepresentation::kWord64);

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

    case IrOpcode::kWord64Shr:
    case IrOpcode::kWord32Shr: {
      MachineRepresentation rep = opcode == IrOpcode::kWord64Shr
                                      ? MachineRepresentation::kWord64
                                      : MachineRepresentation::kWord32;
      return assembler.Shift(Map(node->InputAt(0)), Map(node->InputAt(1)),
                             ShiftOp::Kind::kShiftRightLogical, rep);
    }

    case IrOpcode::kWord64Shl:
    case IrOpcode::kWord32Shl: {
      MachineRepresentation rep = opcode == IrOpcode::kWord64Shl
                                      ? MachineRepresentation::kWord64
                                      : MachineRepresentation::kWord32;
      return assembler.Shift(Map(node->InputAt(0)), Map(node->InputAt(1)),
                             ShiftOp::Kind::kShiftLeft, rep);
    }

    case IrOpcode::kWord32Equal:
      return assembler.Equal(Map(node->InputAt(0)), Map(node->InputAt(1)),
                             MachineRepresentation::kWord32);
    case IrOpcode::kWord64Equal:
      return assembler.Equal(Map(node->InputAt(0)), Map(node->InputAt(1)),
                             MachineRepresentation::kWord64);
    case IrOpcode::kFloat32Equal:
      return assembler.Equal(Map(node->InputAt(0)), Map(node->InputAt(1)),
                             MachineRepresentation::kFloat32);
    case IrOpcode::kFloat64Equal:
      return assembler.Equal(Map(node->InputAt(0)), Map(node->InputAt(1)),
                             MachineRepresentation::kFloat64);

    case IrOpcode::kInt32LessThan:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kSignedLessThan,
                                  MachineRepresentation::kWord32);
    case IrOpcode::kInt64LessThan:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kSignedLessThan,
                                  MachineRepresentation::kWord64);
    case IrOpcode::kUint32LessThan:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kUnsignedLessThan,
                                  MachineRepresentation::kWord32);
    case IrOpcode::kUint64LessThan:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kUnsignedLessThan,
                                  MachineRepresentation::kWord64);
    case IrOpcode::kFloat32LessThan:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kSignedLessThan,
                                  MachineRepresentation::kFloat32);
    case IrOpcode::kFloat64LessThan:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kSignedLessThan,
                                  MachineRepresentation::kFloat64);

    case IrOpcode::kInt32LessThanOrEqual:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kSignedLessThanOrEqual,
                                  MachineRepresentation::kWord32);
    case IrOpcode::kInt64LessThanOrEqual:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kSignedLessThanOrEqual,
                                  MachineRepresentation::kWord64);
    case IrOpcode::kUint32LessThanOrEqual:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kUnsignedLessThanOrEqual,
                                  MachineRepresentation::kWord32);
    case IrOpcode::kUint64LessThanOrEqual:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kUnsignedLessThanOrEqual,
                                  MachineRepresentation::kWord64);
    case IrOpcode::kFloat32LessThanOrEqual:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kSignedLessThanOrEqual,
                                  MachineRepresentation::kFloat32);
    case IrOpcode::kFloat64LessThanOrEqual:
      return assembler.Comparison(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                  ComparisonOp::Kind::kSignedLessThanOrEqual,
                                  MachineRepresentation::kFloat64);

    case IrOpcode::kInt32Add:
      return assembler.Add(Map(node->InputAt(0)), Map(node->InputAt(1)),
                           MachineRepresentation::kWord32);
    case IrOpcode::kInt32AddWithOverflow:
      return assembler.AddWithOverflow(Map(node->InputAt(0)),
                                       Map(node->InputAt(1)),
                                       MachineRepresentation::kWord32);
    case IrOpcode::kInt64Add:
      return assembler.Add(Map(node->InputAt(0)), Map(node->InputAt(1)),
                           MachineRepresentation::kWord64);
    case IrOpcode::kInt64AddWithOverflow:
      return assembler.AddWithOverflow(Map(node->InputAt(0)),
                                       Map(node->InputAt(1)),
                                       MachineRepresentation::kWord64);
    case IrOpcode::kFloat64Add:
      return assembler.Add(Map(node->InputAt(0)), Map(node->InputAt(1)),
                           MachineRepresentation::kFloat64);
    case IrOpcode::kFloat32Add:
      return assembler.Add(Map(node->InputAt(0)), Map(node->InputAt(1)),
                           MachineRepresentation::kFloat32);

    case IrOpcode::kInt32Mul:
      return assembler.Mul(Map(node->InputAt(0)), Map(node->InputAt(1)),
                           MachineRepresentation::kWord32);
    case IrOpcode::kInt32MulWithOverflow:
      return assembler.MulWithOverflow(Map(node->InputAt(0)),
                                       Map(node->InputAt(1)),
                                       MachineRepresentation::kWord32);
    case IrOpcode::kInt64Mul:
      return assembler.Mul(Map(node->InputAt(0)), Map(node->InputAt(1)),
                           MachineRepresentation::kWord64);

    case IrOpcode::kInt32Sub:
      return assembler.Sub(Map(node->InputAt(0)), Map(node->InputAt(1)),
                           MachineRepresentation::kWord32);
    case IrOpcode::kInt32SubWithOverflow:
      return assembler.SubWithOverflow(Map(node->InputAt(0)),
                                       Map(node->InputAt(1)),
                                       MachineRepresentation::kWord32);
    case IrOpcode::kInt64Sub:
      return assembler.Sub(Map(node->InputAt(0)), Map(node->InputAt(1)),
                           MachineRepresentation::kWord64);
    case IrOpcode::kInt64SubWithOverflow:
      return assembler.SubWithOverflow(Map(node->InputAt(0)),
                                       Map(node->InputAt(1)),
                                       MachineRepresentation::kWord64);

    case IrOpcode::kFloat32Abs:
      return assembler.FloatUnary(Map(node->InputAt(0)),
                                  FloatUnaryOp::Kind::kAbs,
                                  MachineRepresentation::kFloat32);
    case IrOpcode::kFloat64Abs:
      return assembler.FloatUnary(Map(node->InputAt(0)),
                                  FloatUnaryOp::Kind::kAbs,
                                  MachineRepresentation::kFloat64);

    case IrOpcode::kFloat32Neg:
      return assembler.FloatUnary(Map(node->InputAt(0)),
                                  FloatUnaryOp::Kind::kNegate,
                                  MachineRepresentation::kFloat32);
    case IrOpcode::kFloat64Neg:
      return assembler.FloatUnary(Map(node->InputAt(0)),
                                  FloatUnaryOp::Kind::kNegate,
                                  MachineRepresentation::kFloat64);

    case IrOpcode::kFloat64SilenceNaN:
      return assembler.FloatUnary(Map(node->InputAt(0)),
                                  FloatUnaryOp::Kind::kSilenceNaN,
                                  MachineRepresentation::kFloat64);

    case IrOpcode::kTruncateInt64ToInt32:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kIntegerTruncate,
          MachineRepresentation::kWord64, MachineRepresentation::kWord32);
    case IrOpcode::kBitcastWord32ToWord64:
      return assembler.Change(Map(node->InputAt(0)), ChangeOp::Kind::kBitcast,
                              MachineRepresentation::kWord32,
                              MachineRepresentation::kWord64);
    case IrOpcode::kChangeUint32ToUint64:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kZeroExtend,
          MachineRepresentation::kWord32, MachineRepresentation::kWord64);
    case IrOpcode::kChangeInt32ToInt64:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kSignExtend,
          MachineRepresentation::kWord32, MachineRepresentation::kWord64);
    case IrOpcode::kChangeInt32ToFloat64:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kSignedToFloat,
          MachineRepresentation::kWord32, MachineRepresentation::kFloat64);
    case IrOpcode::kChangeInt64ToFloat64:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kSignedToFloat,
          MachineRepresentation::kWord64, MachineRepresentation::kFloat64);
    case IrOpcode::kChangeUint32ToFloat64:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kUnsignedToFloat,
          MachineRepresentation::kWord32, MachineRepresentation::kFloat64);
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
    case IrOpcode::kTruncateFloat64ToWord32:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kUnsignedFloatTruncate,
          MachineRepresentation::kFloat64, MachineRepresentation::kWord32);
    case IrOpcode::kRoundFloat64ToInt32:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kSignedFloatTruncate,
          MachineRepresentation::kFloat64, MachineRepresentation::kWord32);
    case IrOpcode::kChangeFloat64ToInt32:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kSignedNarrowing,
          MachineRepresentation::kFloat64, MachineRepresentation::kWord32);
    case IrOpcode::kChangeFloat64ToUint32:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kUnsignedNarrowing,
          MachineRepresentation::kFloat64, MachineRepresentation::kWord32);
    case IrOpcode::kChangeFloat64ToInt64:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kSignedNarrowing,
          MachineRepresentation::kFloat64, MachineRepresentation::kWord64);
    case IrOpcode::kChangeFloat64ToUint64:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kUnsignedNarrowing,
          MachineRepresentation::kFloat64, MachineRepresentation::kWord64);
    case IrOpcode::kFloat64ExtractLowWord32:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kExtractLowHalf,
          MachineRepresentation::kFloat64, MachineRepresentation::kWord32);
    case IrOpcode::kFloat64ExtractHighWord32:
      return assembler.Change(
          Map(node->InputAt(0)), ChangeOp::Kind::kExtractHighHalf,
          MachineRepresentation::kFloat64, MachineRepresentation::kWord32);

    case IrOpcode::kBitcastTaggedToWord:
      return assembler.TaggedBitcast(Map(node->InputAt(0)),
                                     MachineRepresentation::kTagged,
                                     MachineType::PointerRepresentation());
    case IrOpcode::kBitcastWordToTagged:
      return assembler.TaggedBitcast(Map(node->InputAt(0)),
                                     MachineType::PointerRepresentation(),
                                     MachineRepresentation::kTagged);

    case IrOpcode::kLoad: {
      MachineType loaded_rep = LoadRepresentationOf(op);
      Node* base = node->InputAt(0);
      Node* index = node->InputAt(1);
      if (index->opcode() == IrOpcode::kInt32Constant) {
        int32_t offset = OpParameter<int32_t>(index->op());
        return assembler.Load(Map(base), LoadOp::Kind::kRaw, loaded_rep,
                              offset);
      }
      if (index->opcode() == IrOpcode::kInt64Constant) {
        int64_t offset = OpParameter<int64_t>(index->op());
        if (base::IsValueInRangeForNumericType<int32_t>(offset)) {
          return assembler.Load(Map(base), LoadOp::Kind::kRaw, loaded_rep,
                                static_cast<int32_t>(offset));
        }
      }
      int32_t offset = 0;
      uint8_t element_size_log2 = 0;
      return assembler.IndexedLoad(Map(base), Map(index),
                                   IndexedLoadOp::Kind::kRaw, loaded_rep,
                                   offset, element_size_log2);
    }

    case IrOpcode::kStore: {
      StoreRepresentation store_rep = StoreRepresentationOf(op);
      Node* base = node->InputAt(0);
      Node* index = node->InputAt(1);
      Node* value = node->InputAt(2);
      if (index->opcode() == IrOpcode::kInt32Constant) {
        int32_t offset = OpParameter<int32_t>(index->op());
        return assembler.Store(Map(base), Map(value), StoreOp::Kind::kRaw,
                               store_rep.representation(),
                               store_rep.write_barrier_kind(), offset);
      }
      if (index->opcode() == IrOpcode::kInt64Constant) {
        int64_t offset = OpParameter<int64_t>(index->op());
        if (base::IsValueInRangeForNumericType<int32_t>(offset)) {
          return assembler.Store(Map(base), Map(value), StoreOp::Kind::kRaw,
                                 store_rep.representation(),
                                 store_rep.write_barrier_kind(),
                                 static_cast<int32_t>(offset));
        }
      }
      int32_t offset = 0;
      uint8_t element_size_log2 = 0;
      return assembler.IndexedStore(
          Map(base), Map(index), Map(value), IndexedStoreOp::Kind::kRaw,
          store_rep.representation(), store_rep.write_barrier_kind(), offset,
          element_size_log2);
    }

    case IrOpcode::kStackPointerGreaterThan:
      return assembler.StackPointerGreaterThan(Map(node->InputAt(0)),
                                               StackCheckKindOf(op));
    case IrOpcode::kLoadStackCheckOffset:
      return assembler.LoadStackCheckOffset();

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
      if (pop_count->opcode() != IrOpcode::kInt32Constant) {
        UNIMPLEMENTED();
      }
      base::SmallVector<OpIndex, 4> return_values;
      for (int i = 1; i < node->op()->ValueInputCount(); ++i) {
        return_values.push_back(Map(node->InputAt(i)));
      }
      return assembler.Return(base::VectorOf(return_values),
                              OpParameter<int32_t>(pop_count->op()));
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
      switch (input->opcode()) {
        case IrOpcode::kInt32AddWithOverflow:
        case IrOpcode::kInt64AddWithOverflow:
        case IrOpcode::kInt32MulWithOverflow:
        case IrOpcode::kInt32SubWithOverflow:
        case IrOpcode::kInt64SubWithOverflow:
          if (index == 0) {
            return assembler.Projection(Map(input),
                                        ProjectionOp::Kind::kResult);
          } else {
            DCHECK_EQ(index, 1);
            return assembler.Projection(Map(input),
                                        ProjectionOp::Kind::kOverflowBit);
          }
        default:
          UNIMPLEMENTED();
      }
    }

    default:
      std::cout << "unsupported node type: " << *node->op() << "\n";
      node->Print();
      UNIMPLEMENTED();
  }
}

}  // namespace

void BuildGraph(Schedule* schedule, Zone* graph_zone, Zone* phase_zone,
                Graph* graph) {
  GraphBuilder{graph_zone, phase_zone, *schedule, Assembler(graph, phase_zone)}
      .Run();
}

}  // namespace v8::internal::compiler::turboshaft
