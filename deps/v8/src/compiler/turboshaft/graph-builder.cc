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

namespace {

struct GraphBuilder {
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
    OpIndex exit_frame_state;
    OpIndex loop_dominating_frame_state;
    uint8_t use_count_at_loop_entry;
  };
  NodeAuxData<OpIndex> op_mapping{phase_zone};
  ZoneVector<BlockData> block_mapping{schedule.RpoBlockCount(), phase_zone};

  base::Optional<BailoutReason> Run();

 private:
  OpIndex Map(Node* old_node) {
    OpIndex result = op_mapping.Get(old_node);
    DCHECK(assembler.output_graph().IsValid(result));
    return result;
  }
  Block* Map(BasicBlock* block) {
    Block* result = block_mapping[block->rpo_number()].block;
    DCHECK_NOT_NULL(result);
    return result;
  }

  void FixLoopPhis(Block* loop, Block* backedge) {
    DCHECK(loop->IsLoop());
    for (Operation& op : assembler.output_graph().operations(*loop)) {
      if (!op.Is<PendingLoopPhiOp>()) continue;
      auto& pending_phi = op.Cast<PendingLoopPhiOp>();
      assembler.output_graph().Replace<PhiOp>(
          assembler.output_graph().Index(pending_phi),
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
                  const base::SmallVector<int, 16>& predecessor_permutation,
                  OpIndex& dominating_frame_state,
                  base::Optional<BailoutReason>* bailout,
                  bool is_final_control = false);

  OpIndex EmitProjectionsAndTuple(OpIndex op_idx) {
    Operation& op = assembler.output_graph().Get(op_idx);
    base::Vector<const RegisterRepresentation> outputs_rep = op.outputs_rep();
    if (outputs_rep.size() <= 1) {
      // If {op} has a single output, there is no need to emit Projections or
      // Tuple, so we just return it.
      return op_idx;
    }
    base::SmallVector<OpIndex, 16> tuple_inputs;
    for (size_t i = 0; i < outputs_rep.size(); i++) {
      tuple_inputs.push_back(assembler.Projection(op_idx, i, outputs_rep[i]));
    }
    return assembler.Tuple(base::VectorOf(tuple_inputs));
  }
};

base::Optional<BailoutReason> GraphBuilder::Run() {
  for (BasicBlock* block : *schedule.rpo_order()) {
    block_mapping[block->rpo_number()] = {
        block->IsLoopHeader() ? assembler.NewLoopHeader()
                              : assembler.NewBlock(),
        OpIndex::Invalid(), OpIndex::Invalid(), 0};
  }

  for (BasicBlock* block : *schedule.rpo_order()) {
    Block* target_block = Map(block);
    if (!assembler.Bind(target_block)) continue;

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

    // Compute the dominating frame state. If all predecessors deliver the same
    // FrameState, we use this. Otherwise we rely on a new Checkpoint before any
    // operation that needs a FrameState.
    OpIndex dominating_frame_state = OpIndex::Invalid();
    if (!predecessors.empty()) {
      // Predecessor[0] cannot be a backedge.
      DCHECK_LT(predecessors[0]->rpo_number(), block->rpo_number());
      dominating_frame_state =
          block_mapping[predecessors[0]->rpo_number()].exit_frame_state;
      for (size_t i = 1; i < predecessors.size(); ++i) {
        if (predecessors[i]->rpo_number() > block->rpo_number() &&
            dominating_frame_state.valid()) {
          // This is a backedge, so we do not have an exit_frame_state yet.
          // We record this and later verify that the backedge provides this, if
          // this FrameState has a use inside the loop.
          DCHECK(target_block->IsLoop());
          DCHECK_EQ(i, PhiOp::kLoopPhiBackEdgeIndex);
          auto& mapping = block_mapping[block->rpo_number()];
          mapping.loop_dominating_frame_state = dominating_frame_state;
          mapping.use_count_at_loop_entry = assembler.output_graph()
                                                .Get(dominating_frame_state)
                                                .saturated_use_count;
          DCHECK_EQ(i, predecessors.size() - 1);
          break;
        }
        if (dominating_frame_state !=
            block_mapping[predecessors[i]->rpo_number()].exit_frame_state) {
          dominating_frame_state = OpIndex::Invalid();
          break;
        }
      }
    }

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
        assembler.Goto(destination);
        if (destination->IsBound()) {
          DCHECK(destination->IsLoop());
          FixLoopPhis(destination, target_block);
          auto& mapping = block_mapping[block->SuccessorAt(0)->rpo_number()];
          if (mapping.loop_dominating_frame_state.valid()) {
            uint8_t final_use_count =
                assembler.output_graph()
                    .Get(mapping.loop_dominating_frame_state)
                    .saturated_use_count;
            if (final_use_count > mapping.use_count_at_loop_entry) {
              DCHECK_EQ(dominating_frame_state,
                        mapping.loop_dominating_frame_state);
            }
          }
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
    DCHECK_NULL(assembler.current_block());

    block_mapping[block->rpo_number()].exit_frame_state =
        dominating_frame_state;
  }

  if (source_positions->IsEnabled()) {
    for (OpIndex index : assembler.output_graph().AllOperationIndices()) {
      compiler::NodeId origin = assembler.output_graph()
                                    .operation_origins()[index]
                                    .DecodeTurbofanNodeId();
      assembler.output_graph().source_positions()[index] =
          source_positions->GetSourcePosition(origin);
    }
  }

  if (origins) {
    for (OpIndex index : assembler.output_graph().AllOperationIndices()) {
      OpIndex origin = assembler.output_graph().operation_origins()[index];
      origins->SetNodeOrigin(index.id(), origin.DecodeTurbofanNodeId());
    }
  }

  return base::nullopt;
}

OpIndex GraphBuilder::Process(
    Node* node, BasicBlock* block,
    const base::SmallVector<int, 16>& predecessor_permutation,
    OpIndex& dominating_frame_state, base::Optional<BailoutReason>* bailout,
    bool is_final_control) {
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
      return OpIndex::Invalid();

    case IrOpcode::kCheckpoint: {
      // Preserve the frame state from this checkpoint for following nodes.
      dominating_frame_state = Map(NodeProperties::GetFrameStateInput(node));
      return OpIndex::Invalid();
    }

    case IrOpcode::kIfException: {
      return assembler.LoadException();
    }

    case IrOpcode::kIfSuccess: {
      // We emit all of the value projections of the call now, emit a Tuple with
      // all of those projections, and remap the old call to this new Tuple
      // instead of the CallAndCatchExceptionOp.
      Node* call = node->InputAt(0);
      DCHECK_EQ(call->opcode(), IrOpcode::kCall);
      OpIndex call_idx = Map(call);
      CallAndCatchExceptionOp& op = assembler.output_graph()
                                        .Get(call_idx)
                                        .Cast<CallAndCatchExceptionOp>();

      size_t return_count = op.outputs_rep().size();
      DCHECK_EQ(return_count, op.descriptor->descriptor->ReturnCount());
      if (return_count <= 1) {
        // Calls with one output (or zero) do not require Projections.
        return OpIndex::Invalid();
      }
      base::Vector<OpIndex> projections =
          graph_zone->NewVector<OpIndex>(return_count);
      for (size_t i = 0; i < return_count; i++) {
        projections[i] = assembler.Projection(call_idx, i, op.outputs_rep()[i]);
      }
      OpIndex tuple_idx = assembler.Tuple(projections);

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
      return assembler.Parameter(info.index(), rep, info.debug_name());
    }

    case IrOpcode::kOsrValue: {
      return assembler.OsrValue(OsrValueIndexOf(op));
    }

    case IrOpcode::kPhi: {
      int input_count = op->ValueInputCount();
      RegisterRepresentation rep =
          RegisterRepresentation::FromMachineRepresentation(
              PhiRepresentationOf(op));
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
    case IrOpcode::kRelocatableInt64Constant:
      return assembler.RelocatableConstant(
          OpParameter<RelocatablePtrConstantInfo>(op).value(),
          OpParameter<RelocatablePtrConstantInfo>(op).rmode());
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

#define TUPLE_BINOP_CASE(opcode, assembler_op)                                \
  case IrOpcode::k##opcode: {                                                 \
    OpIndex idx =                                                             \
        assembler.assembler_op(Map(node->InputAt(0)), Map(node->InputAt(1))); \
    return EmitProjectionsAndTuple(idx);                                      \
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
      return assembler.Shift(Map(node->InputAt(0)), Map(node->InputAt(1)), kind,
                             rep);
    }

#define UNARY_CASE(opcode, assembler_op) \
  case IrOpcode::k##opcode:              \
    return assembler.assembler_op(Map(node->InputAt(0)));
#define TUPLE_UNARY_CASE(opcode, assembler_op)                   \
  case IrOpcode::k##opcode: {                                    \
    OpIndex idx = assembler.assembler_op(Map(node->InputAt(0))); \
    return EmitProjectionsAndTuple(idx);                         \
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
          return assembler.TruncateFloat32ToInt32OverflowUndefined(
              Map(node->InputAt(0)));
        case TruncateKind::kSetOverflowToMin:
          return assembler.TruncateFloat32ToInt32OverflowToMin(
              Map(node->InputAt(0)));
      }
    case IrOpcode::kTruncateFloat32ToUint32:
      switch (OpParameter<TruncateKind>(node->op())) {
        case TruncateKind::kArchitectureDefault:
          return assembler.TruncateFloat32ToUint32OverflowUndefined(
              Map(node->InputAt(0)));
        case TruncateKind::kSetOverflowToMin:
          return assembler.TruncateFloat32ToUint32OverflowToMin(
              Map(node->InputAt(0)));
      }
    case IrOpcode::kTruncateFloat64ToInt64:
      switch (OpParameter<TruncateKind>(node->op())) {
        case TruncateKind::kArchitectureDefault:
          return assembler.TruncateFloat64ToInt64OverflowUndefined(
              Map(node->InputAt(0)));
        case TruncateKind::kSetOverflowToMin:
          return assembler.TruncateFloat64ToInt64OverflowToMin(
              Map(node->InputAt(0)));
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
                                     RegisterRepresentation::Tagged(),
                                     RegisterRepresentation::PointerSized());
    case IrOpcode::kBitcastWordToTagged:
      return assembler.TaggedBitcast(Map(node->InputAt(0)),
                                     RegisterRepresentation::PointerSized(),
                                     RegisterRepresentation::Tagged());

#define OBJECT_IS_CASE(kind)                                        \
  case IrOpcode::kObjectIs##kind: {                                 \
    return assembler.ObjectIs(Map(node->InputAt(0)),                \
                              ObjectIsOp::Kind::k##kind,            \
                              ObjectIsOp::InputAssumptions::kNone); \
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

    case IrOpcode::kCheckBigInt: {
      DCHECK(dominating_frame_state.valid());
      OpIndex input = Map(node->InputAt(0));
      OpIndex check = assembler.ObjectIs(input, ObjectIsOp::Kind::kBigInt,
                                         ObjectIsOp::InputAssumptions::kNone);
      assembler.DeoptimizeIfNot(check, dominating_frame_state,
                                DeoptimizeReason::kNotABigInt,
                                CheckParametersOf(op).feedback());
      return input;
    }
    case IrOpcode::kCheckedBigIntToBigInt64: {
      DCHECK(dominating_frame_state.valid());
      OpIndex input = Map(node->InputAt(0));
      OpIndex check =
          assembler.ObjectIs(Map(node->InputAt(0)), ObjectIsOp::Kind::kBigInt64,
                             ObjectIsOp::InputAssumptions::kBigInt);
      assembler.DeoptimizeIfNot(check, dominating_frame_state,
                                DeoptimizeReason::kNotABigInt64,
                                CheckParametersOf(op).feedback());
      return input;
    }

#define CHANGE_CASE(name, kind, input_rep, input_interpretation)         \
  case IrOpcode::k##name:                                                \
    return assembler.ConvertToObject(                                    \
        Map(node->InputAt(0)), ConvertToObjectOp::Kind::k##kind,         \
        input_rep::Rep,                                                  \
        ConvertToObjectOp::InputInterpretation::k##input_interpretation, \
        CheckForMinusZeroMode::kDontCheckForMinusZero);
      CHANGE_CASE(ChangeInt32ToTagged, Number, Word32, Signed)
      CHANGE_CASE(ChangeUint32ToTagged, Number, Word32, Unsigned)
      CHANGE_CASE(ChangeInt64ToTagged, Number, Word64, Signed)
      CHANGE_CASE(ChangeUint64ToTagged, Number, Word64, Unsigned)
      CHANGE_CASE(ChangeFloat64ToTaggedPointer, HeapNumber, Float64, Signed)
      CHANGE_CASE(ChangeInt64ToBigInt, BigInt, Word64, Signed)
      CHANGE_CASE(ChangeUint64ToBigInt, BigInt, Word64, Unsigned)
      CHANGE_CASE(ChangeInt31ToTaggedSigned, Smi, Word32, Signed)
      CHANGE_CASE(ChangeBitToTagged, Boolean, Word32, Signed)
      CHANGE_CASE(StringFromSingleCharCode, String, Word32, CharCode)
      CHANGE_CASE(StringFromSingleCodePoint, String, Word32, CodePoint)

    case IrOpcode::kChangeFloat64ToTagged:
      return assembler.ConvertToObject(
          Map(node->InputAt(0)), ConvertToObjectOp::Kind::kNumber,
          RegisterRepresentation::Float64(),
          ConvertToObjectOp::InputInterpretation::kSigned,
          CheckMinusZeroModeOf(node->op()));
#undef CHANGE_CASE

    case IrOpcode::kSelect: {
      OpIndex cond = Map(node->InputAt(0));
      OpIndex vtrue = Map(node->InputAt(1));
      OpIndex vfalse = Map(node->InputAt(2));
      const SelectParameters& params = SelectParametersOf(op);
      return assembler.Select(cond, vtrue, vfalse,
                              RegisterRepresentation::FromMachineRepresentation(
                                  params.representation()),
                              params.hint(), SelectOp::Implementation::kBranch);
    }
    case IrOpcode::kWord32Select:
      return assembler.Select(
          Map(node->InputAt(0)), Map(node->InputAt(1)), Map(node->InputAt(2)),
          RegisterRepresentation::Word32(), BranchHint::kNone,
          SelectOp::Implementation::kCMove);
    case IrOpcode::kWord64Select:
      return assembler.Select(
          Map(node->InputAt(0)), Map(node->InputAt(1)), Map(node->InputAt(2)),
          RegisterRepresentation::Word64(), BranchHint::kNone,
          SelectOp::Implementation::kCMove);

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
      return assembler.Load(Map(base), Map(index), kind, loaded_rep, offset,
                            element_size_log2);
    }
    case IrOpcode::kProtectedLoad: {
      MemoryRepresentation loaded_rep =
          MemoryRepresentation::FromMachineType(LoadRepresentationOf(op));
      return assembler.Load(Map(node->InputAt(0)), Map(node->InputAt(1)),
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
        assembler.Store(Map(base), Map(value), kind,
                        MemoryRepresentation::FromMachineRepresentation(
                            store_rep.representation()),
                        store_rep.write_barrier_kind(), offset);
        return OpIndex::Invalid();
      }
      if (index->opcode() == IrOpcode::kInt64Constant) {
        int64_t offset = OpParameter<int64_t>(index->op());
        if (base::IsValueInRangeForNumericType<int32_t>(offset)) {
          assembler.Store(Map(base), Map(value), kind,
                          MemoryRepresentation::FromMachineRepresentation(
                              store_rep.representation()),
                          store_rep.write_barrier_kind(),
                          static_cast<int32_t>(offset));
          return OpIndex::Invalid();
        }
      }
      int32_t offset = 0;
      uint8_t element_size_log2 = 0;
      assembler.Store(Map(base), Map(index), Map(value), kind,
                      MemoryRepresentation::FromMachineRepresentation(
                          store_rep.representation()),
                      store_rep.write_barrier_kind(), offset,
                      element_size_log2);
      return OpIndex::Invalid();
    }
    case IrOpcode::kProtectedStore:
      assembler.Store(Map(node->InputAt(0)), Map(node->InputAt(1)),
                      Map(node->InputAt(2)), StoreOp::Kind::Protected(),
                      MemoryRepresentation::FromMachineRepresentation(
                          OpParameter<MachineRepresentation>(node->op())),
                      WriteBarrierKind::kNoWriteBarrier);
      return OpIndex::Invalid();

    case IrOpcode::kRetain:
      assembler.Retain(Map(node->InputAt(0)));
      return OpIndex::Invalid();
    case IrOpcode::kStackPointerGreaterThan:
      return assembler.StackPointerGreaterThan(Map(node->InputAt(0)),
                                               StackCheckKindOf(op));
    case IrOpcode::kLoadStackCheckOffset:
      return assembler.StackCheckOffset();
    case IrOpcode::kLoadFramePointer:
      return assembler.FramePointer();
    case IrOpcode::kLoadParentFramePointer:
      return assembler.ParentFramePointer();

    case IrOpcode::kStackSlot:
      return assembler.StackSlot(StackSlotRepresentationOf(op).size(),
                                 StackSlotRepresentationOf(op).alignment());

    case IrOpcode::kBranch:
      DCHECK_EQ(block->SuccessorCount(), 2);
      assembler.Branch(Map(node->InputAt(0)), Map(block->SuccessorAt(0)),
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
      assembler.Switch(
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
        return EmitProjectionsAndTuple(assembler.Call(
            callee, frame_state_idx, base::VectorOf(arguments), ts_descriptor));
      } else {
        DCHECK_EQ(block->SuccessorCount(), 2);

        Block* if_success = Map(block->SuccessorAt(0));
        Block* if_exception = Map(block->SuccessorAt(1));
        // CallAndCatchException is a block terminator, so we can't generate the
        // projections right away. We'll generate them in the IfSuccess
        // successor.
        return assembler.CallAndCatchException(
            callee, frame_state_idx, base::VectorOf(arguments), if_success,
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

      assembler.TailCall(callee, base::VectorOf(arguments), ts_descriptor);
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
      return assembler.FrameState(
          builder.Inputs(), builder.inlined(),
          builder.AllocateFrameStateData(frame_state.frame_state_info(),
                                         graph_zone));
    }

    case IrOpcode::kDeoptimizeIf:
      assembler.DeoptimizeIf(Map(node->InputAt(0)), Map(node->InputAt(1)),
                             &DeoptimizeParametersOf(op));
      return OpIndex::Invalid();
    case IrOpcode::kDeoptimizeUnless:
      assembler.DeoptimizeIfNot(Map(node->InputAt(0)), Map(node->InputAt(1)),
                                &DeoptimizeParametersOf(op));
      return OpIndex::Invalid();

    case IrOpcode::kTrapIf:
      assembler.TrapIf(Map(node->InputAt(0)), TrapIdOf(op));
      return OpIndex::Invalid();
    case IrOpcode::kTrapUnless:
      assembler.TrapIfNot(Map(node->InputAt(0)), TrapIdOf(op));
      return OpIndex::Invalid();

    case IrOpcode::kDeoptimize: {
      OpIndex frame_state = Map(node->InputAt(0));
      assembler.Deoptimize(frame_state, &DeoptimizeParametersOf(op));
      return OpIndex::Invalid();
    }

    case IrOpcode::kReturn: {
      Node* pop_count = node->InputAt(0);
      base::SmallVector<OpIndex, 4> return_values;
      for (int i = 1; i < node->op()->ValueInputCount(); ++i) {
        return_values.push_back(Map(node->InputAt(i)));
      }
      assembler.Return(Map(pop_count), base::VectorOf(return_values));
      return OpIndex::Invalid();
    }

    case IrOpcode::kUnreachable:
      for (Node* use : node->uses()) {
        CHECK_EQ(use->opcode(), IrOpcode::kThrow);
      }
      return OpIndex::Invalid();
    case IrOpcode::kThrow:
      assembler.Unreachable();
      return OpIndex::Invalid();

    case IrOpcode::kProjection: {
      Node* input = node->InputAt(0);
      size_t index = ProjectionIndexOf(op);
      RegisterRepresentation rep =
          RegisterRepresentation::FromMachineRepresentation(
              NodeProperties::GetProjectionType(node));
      return assembler.Projection(Map(input), index, rep);
    }

    case IrOpcode::kStaticAssert: {
      // We currently ignore StaticAsserts in turboshaft (because some of them
      // need specific unported optimizations to be evaluated).
      // TODO(turboshaft): once CommonOperatorReducer and MachineOperatorReducer
      // have been ported, re-enable StaticAsserts.
      // return assembler.ReduceStaticAssert(Map(node->InputAt(0)),
      //                                     StaticAssertSourceOf(node->op()));
      return OpIndex::Invalid();
    }

    case IrOpcode::kAllocate: {
      AllocationType allocation = AllocationTypeOf(node->op());
      return assembler.Allocate(Map(node->InputAt(0)), allocation,
                                AllowLargeObjects::kFalse);
    }
    // TODO(nicohartmann@): We might not see AllocateRaw here anymore.
    case IrOpcode::kAllocateRaw: {
      Node* size = node->InputAt(0);
      const AllocateParameters& params = AllocateParametersOf(node->op());
      return assembler.Allocate(Map(size), params.allocation_type(),
                                params.allow_large_objects());
    }
    case IrOpcode::kStoreToObject: {
      Node* object = node->InputAt(0);
      Node* offset = node->InputAt(1);
      Node* value = node->InputAt(2);
      ObjectAccess const& access = ObjectAccessOf(node->op());
      assembler.Store(
          Map(object), Map(offset), Map(value), StoreOp::Kind::TaggedBase(),
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
      assembler.Store(Map(object), Map(index), Map(value), kind, rep,
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
        value = assembler.ShiftLeft(value, kBoundedSizeShift,
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
      assembler.Store(object, value, kind, rep, access.write_barrier_kind,
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
      return assembler.Load(Map(object), Map(offset),
                            LoadOp::Kind::TaggedBase(), rep, kHeapObjectTag);
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
      OpIndex value = assembler.Load(Map(object), kind, rep, access.offset);
#ifdef V8_ENABLE_SANDBOX
      if (is_sandboxed_external) {
        value =
            assembler.DecodeExternalPointer(value, access.external_pointer_tag);
      }
      if (access.is_bounded_size_access) {
        DCHECK(!is_sandboxed_external);
        value = assembler.ShiftRightLogical(value, kBoundedSizeShift,
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
      return assembler.Load(Map(object), Map(index), kind, rep,
                            access.header_size, rep.SizeInBytesLog2());
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
          assembler.output_graph().Get(input_index).outputs_rep()[0];
      return assembler.CheckTurboshaftTypeOf(input_index, rep, *type_opt,
                                             false);
    }

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
  GraphBuilder builder{broker,
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

}  // namespace v8::internal::compiler::turboshaft
