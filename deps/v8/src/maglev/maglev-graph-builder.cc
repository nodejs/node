// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-builder.h"

#include "src/base/optional.h"
#include "src/base/v8-fallthrough.h"
#include "src/builtins/builtins-constructor.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/common/globals.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/processed-feedback.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/handles/maybe-handles-inl.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecodes.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/elements-kind.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/property-cell.h"
#include "src/objects/property-details.h"
#include "src/objects/slots-inl.h"

namespace v8 {
namespace internal {

namespace maglev {

MaglevGraphBuilder::MaglevGraphBuilder(LocalIsolate* local_isolate,
                                       MaglevCompilationUnit* compilation_unit,
                                       Graph* graph, MaglevGraphBuilder* parent)
    : local_isolate_(local_isolate),
      compilation_unit_(compilation_unit),
      parent_(parent),
      graph_(graph),
      iterator_(bytecode().object()),
      // Add an extra jump_target slot for the inline exit if needed.
      jump_targets_(zone()->NewArray<BasicBlockRef>(bytecode().length() +
                                                    (is_inline() ? 1 : 0))),
      // Overallocate merge_states_ by one to allow always looking up the
      // next offset. This overallocated slot can also be used for the inline
      // exit when needed.
      merge_states_(zone()->NewArray<MergePointInterpreterFrameState*>(
          bytecode().length() + 1)),
      current_interpreter_frame_(*compilation_unit_),
      catch_block_stack_(zone()) {
  memset(merge_states_, 0,
         (bytecode().length() + 1) * sizeof(InterpreterFrameState*));
  // Default construct basic block refs.
  // TODO(leszeks): This could be a memset of nullptr to ..._jump_targets_.
  for (int i = 0; i < bytecode().length(); ++i) {
    new (&jump_targets_[i]) BasicBlockRef();
  }

  if (is_inline()) {
    DCHECK_NOT_NULL(parent_);
    DCHECK_GT(compilation_unit->inlining_depth(), 0);
    // The allocation/initialisation logic here relies on inline_exit_offset
    // being the offset one past the end of the bytecode.
    DCHECK_EQ(inline_exit_offset(), bytecode().length());
    merge_states_[inline_exit_offset()] = nullptr;
    new (&jump_targets_[inline_exit_offset()]) BasicBlockRef();
  }

  CalculatePredecessorCounts();
}

void MaglevGraphBuilder::StartPrologue() {
  current_block_ = zone()->New<BasicBlock>(nullptr);
  block_offset_ = -1;
}

BasicBlock* MaglevGraphBuilder::EndPrologue() {
  BasicBlock* first_block = CreateBlock<Jump>({}, &jump_targets_[0]);
  MergeIntoFrameState(first_block, 0);
  return first_block;
}

void MaglevGraphBuilder::SetArgument(int i, ValueNode* value) {
  interpreter::Register reg = interpreter::Register::FromParameterIndex(i);
  current_interpreter_frame_.set(reg, value);
}

ValueNode* MaglevGraphBuilder::GetTaggedArgument(int i) {
  interpreter::Register reg = interpreter::Register::FromParameterIndex(i);
  return GetTaggedValue(reg);
}

void MaglevGraphBuilder::BuildRegisterFrameInitialization() {
  // TODO(leszeks): Extract out a separate "incoming context/closure" nodes,
  // to be able to read in the machine register but also use the frame-spilled
  // slot.
  interpreter::Register regs[] = {interpreter::Register::current_context(),
                                  interpreter::Register::function_closure()};
  for (interpreter::Register& reg : regs) {
    current_interpreter_frame_.set(reg, AddNewNode<InitialValue>({}, reg));
  }

  interpreter::Register new_target_or_generator_register =
      bytecode().incoming_new_target_or_generator_register();

  int register_index = 0;
  // TODO(leszeks): Don't emit if not needed.
  ValueNode* undefined_value = GetRootConstant(RootIndex::kUndefinedValue);
  if (new_target_or_generator_register.is_valid()) {
    int new_target_index = new_target_or_generator_register.index();
    for (; register_index < new_target_index; register_index++) {
      StoreRegister(interpreter::Register(register_index), undefined_value);
    }
    StoreRegister(
        new_target_or_generator_register,
        // TODO(leszeks): Expose in Graph.
        AddNewNode<RegisterInput>({}, kJavaScriptCallNewTargetRegister));
    register_index++;
  }
  for (; register_index < register_count(); register_index++) {
    StoreRegister(interpreter::Register(register_index), undefined_value);
  }
}

void MaglevGraphBuilder::BuildMergeStates() {
  for (auto& offset_and_info : bytecode_analysis().GetLoopInfos()) {
    int offset = offset_and_info.first;
    const compiler::LoopInfo& loop_info = offset_and_info.second;
    const compiler::BytecodeLivenessState* liveness = GetInLivenessFor(offset);
    DCHECK_NULL(merge_states_[offset]);
    if (FLAG_trace_maglev_graph_building) {
      std::cout << "- Creating loop merge state at @" << offset << std::endl;
    }
    merge_states_[offset] = MergePointInterpreterFrameState::NewForLoop(
        *compilation_unit_, offset, NumPredecessors(offset), liveness,
        &loop_info);
  }

  if (bytecode().handler_table_size() > 0) {
    HandlerTable table(*bytecode().object());
    for (int i = 0; i < table.NumberOfRangeEntries(); i++) {
      const int offset = table.GetRangeHandler(i);
      const interpreter::Register context_reg(table.GetRangeData(i));
      const compiler::BytecodeLivenessState* liveness =
          GetInLivenessFor(offset);
      DCHECK_EQ(NumPredecessors(offset), 0);
      DCHECK_NULL(merge_states_[offset]);
      if (FLAG_trace_maglev_graph_building) {
        std::cout << "- Creating exception merge state at @" << offset
                  << ", context register r" << context_reg.index() << std::endl;
      }
      merge_states_[offset] = MergePointInterpreterFrameState::NewForCatchBlock(
          *compilation_unit_, liveness, offset, context_reg, graph_,
          is_inline());
    }
  }
}

namespace {
template <Operation kOperation>
struct NodeForOperationHelper;

#define NODE_FOR_OPERATION_HELPER(Name)               \
  template <>                                         \
  struct NodeForOperationHelper<Operation::k##Name> { \
    using generic_type = Generic##Name;               \
  };
OPERATION_LIST(NODE_FOR_OPERATION_HELPER)
#undef NODE_FOR_OPERATION_HELPER

template <Operation kOperation>
using GenericNodeForOperation =
    typename NodeForOperationHelper<kOperation>::generic_type;

// TODO(victorgomes): Remove this once all operations have fast paths.
template <Operation kOperation>
bool BinaryOperationHasInt32FastPath() {
  switch (kOperation) {
    case Operation::kAdd:
    case Operation::kSubtract:
    case Operation::kMultiply:
    case Operation::kDivide:
    case Operation::kBitwiseAnd:
    case Operation::kBitwiseOr:
    case Operation::kBitwiseXor:
    case Operation::kShiftLeft:
    case Operation::kShiftRight:
    case Operation::kShiftRightLogical:
    case Operation::kEqual:
    case Operation::kStrictEqual:
    case Operation::kLessThan:
    case Operation::kLessThanOrEqual:
    case Operation::kGreaterThan:
    case Operation::kGreaterThanOrEqual:
      return true;
    default:
      return false;
  }
}
template <Operation kOperation>
bool BinaryOperationHasFloat64FastPath() {
  switch (kOperation) {
    case Operation::kAdd:
    case Operation::kSubtract:
    case Operation::kMultiply:
    case Operation::kDivide:
      return true;
    default:
      return false;
  }
}

}  // namespace

// MAP_OPERATION_TO_NODES are tuples with the following format:
// - Operation name,
// - Int32 operation node,
// - Identity of int32 operation (e.g, 0 for add/sub and 1 for mul/div), if it
//   exists, or otherwise {}.
#define MAP_OPERATION_TO_INT32_NODE(V)      \
  V(Add, Int32AddWithOverflow, 0)           \
  V(Subtract, Int32SubtractWithOverflow, 0) \
  V(Multiply, Int32MultiplyWithOverflow, 1) \
  V(Divide, Int32DivideWithOverflow, 1)     \
  V(BitwiseAnd, Int32BitwiseAnd, ~0)        \
  V(BitwiseOr, Int32BitwiseOr, 0)           \
  V(BitwiseXor, Int32BitwiseXor, 0)         \
  V(ShiftLeft, Int32ShiftLeft, 0)           \
  V(ShiftRight, Int32ShiftRight, 0)         \
  V(ShiftRightLogical, Int32ShiftRightLogical, {})

#define MAP_COMPARE_OPERATION_TO_INT32_NODE(V) \
  V(Equal, Int32Equal)                         \
  V(StrictEqual, Int32StrictEqual)             \
  V(LessThan, Int32LessThan)                   \
  V(LessThanOrEqual, Int32LessThanOrEqual)     \
  V(GreaterThan, Int32GreaterThan)             \
  V(GreaterThanOrEqual, Int32GreaterThanOrEqual)

// MAP_OPERATION_TO_FLOAT64_NODE are tuples with the following format:
// (Operation name, Float64 operation node).
#define MAP_OPERATION_TO_FLOAT64_NODE(V) \
  V(Add, Float64Add)                     \
  V(Subtract, Float64Subtract)           \
  V(Multiply, Float64Multiply)           \
  V(Divide, Float64Divide)

template <Operation kOperation>
static constexpr base::Optional<int> Int32Identity() {
  switch (kOperation) {
#define CASE(op, _, identity) \
  case Operation::k##op:      \
    return identity;
    MAP_OPERATION_TO_INT32_NODE(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

template <Operation kOperation>
ValueNode* MaglevGraphBuilder::AddNewInt32BinaryOperationNode(
    std::initializer_list<ValueNode*> inputs) {
  switch (kOperation) {
#define CASE(op, OpNode, _) \
  case Operation::k##op:    \
    return AddNewNode<OpNode>(inputs);
    MAP_OPERATION_TO_INT32_NODE(CASE)
#undef CASE
#define CASE(op, OpNode) \
  case Operation::k##op: \
    return AddNewNode<OpNode>(inputs);
    MAP_COMPARE_OPERATION_TO_INT32_NODE(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

template <Operation kOperation>
ValueNode* MaglevGraphBuilder::AddNewFloat64BinaryOperationNode(
    std::initializer_list<ValueNode*> inputs) {
  switch (kOperation) {
#define CASE(op, OpNode) \
  case Operation::k##op: \
    return AddNewNode<OpNode>(inputs);
    MAP_OPERATION_TO_FLOAT64_NODE(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildGenericUnaryOperationNode() {
  FeedbackSlot slot_index = GetSlotOperand(0);
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<GenericNodeForOperation<kOperation>>(
      {value}, compiler::FeedbackSource{feedback(), slot_index}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildGenericBinaryOperationNode() {
  ValueNode* left = LoadRegisterTagged(0);
  ValueNode* right = GetAccumulatorTagged();
  FeedbackSlot slot_index = GetSlotOperand(1);
  SetAccumulator(AddNewNode<GenericNodeForOperation<kOperation>>(
      {left, right}, compiler::FeedbackSource{feedback(), slot_index}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildGenericBinarySmiOperationNode() {
  ValueNode* left = GetAccumulatorTagged();
  int constant = iterator_.GetImmediateOperand(0);
  ValueNode* right = GetSmiConstant(constant);
  FeedbackSlot slot_index = GetSlotOperand(1);
  SetAccumulator(AddNewNode<GenericNodeForOperation<kOperation>>(
      {left, right}, compiler::FeedbackSource{feedback(), slot_index}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildInt32BinaryOperationNode() {
  // TODO(v8:7700): Do constant folding.
  ValueNode *left, *right;
  if (IsRegisterEqualToAccumulator(0)) {
    left = right = LoadRegisterInt32(0);
  } else {
    left = LoadRegisterInt32(0);
    right = GetAccumulatorInt32();
  }
  SetAccumulator(AddNewInt32BinaryOperationNode<kOperation>({left, right}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildInt32BinarySmiOperationNode() {
  // TODO(v8:7700): Do constant folding.
  ValueNode* left = GetAccumulatorInt32();
  int32_t constant = iterator_.GetImmediateOperand(0);
  if (base::Optional<int>(constant) == Int32Identity<kOperation>()) {
    // If the constant is the unit of the operation, it already has the right
    // value, so we can just return.
    return;
  }
  ValueNode* right = GetInt32Constant(constant);
  SetAccumulator(AddNewInt32BinaryOperationNode<kOperation>({left, right}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildFloat64BinarySmiOperationNode() {
  // TODO(v8:7700): Do constant folding.
  ValueNode* left = GetAccumulatorFloat64();
  double constant = static_cast<double>(iterator_.GetImmediateOperand(0));
  ValueNode* right = GetFloat64Constant(constant);
  SetAccumulator(AddNewFloat64BinaryOperationNode<kOperation>({left, right}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildFloat64BinaryOperationNode() {
  // TODO(v8:7700): Do constant folding.
  ValueNode *left, *right;
  if (IsRegisterEqualToAccumulator(0)) {
    left = right = LoadRegisterFloat64(0);
  } else {
    left = LoadRegisterFloat64(0);
    right = GetAccumulatorFloat64();
  }
  SetAccumulator(AddNewFloat64BinaryOperationNode<kOperation>({left, right}));
}

template <Operation kOperation>
void MaglevGraphBuilder::VisitUnaryOperation() {
  // TODO(victorgomes): Use feedback info and create optimized versions.
  BuildGenericUnaryOperationNode<kOperation>();
}

template <Operation kOperation>
void MaglevGraphBuilder::VisitBinaryOperation() {
  FeedbackNexus nexus = FeedbackNexusForOperand(1);
  switch (nexus.GetBinaryOperationFeedback()) {
    case BinaryOperationHint::kNone:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForBinaryOperation);
      return;
    case BinaryOperationHint::kSignedSmall:
      if (BinaryOperationHasInt32FastPath<kOperation>()) {
        BuildInt32BinaryOperationNode<kOperation>();
        return;
      }
      break;
    case BinaryOperationHint::kSignedSmallInputs:
    case BinaryOperationHint::kNumber:
      if (BinaryOperationHasFloat64FastPath<kOperation>()) {
        BuildFloat64BinaryOperationNode<kOperation>();
        return;
        // } else if (BinaryOperationHasInt32FastPath<kOperation>()) {
        //   // Fall back to int32 fast path if there is one (this will be the
        //   case
        //   // for operations that deal with bits rather than numbers).
        //   BuildInt32BinaryOperationNode<kOperation>();
        //   return;
      }
      break;
    default:
      // Fallback to generic node.
      break;
  }
  BuildGenericBinaryOperationNode<kOperation>();
}

template <Operation kOperation>
void MaglevGraphBuilder::VisitBinarySmiOperation() {
  FeedbackNexus nexus = FeedbackNexusForOperand(1);
  switch (nexus.GetBinaryOperationFeedback()) {
    case BinaryOperationHint::kNone:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForBinaryOperation);
      return;
    case BinaryOperationHint::kSignedSmall:
      if (BinaryOperationHasInt32FastPath<kOperation>()) {
        BuildInt32BinarySmiOperationNode<kOperation>();
        return;
      }
      break;
    case BinaryOperationHint::kSignedSmallInputs:
    case BinaryOperationHint::kNumber:
      if (BinaryOperationHasFloat64FastPath<kOperation>()) {
        BuildFloat64BinarySmiOperationNode<kOperation>();
        return;
        // } else if (BinaryOperationHasInt32FastPath<kOperation>()) {
        //   // Fall back to int32 fast path if there is one (this will be the
        //   case
        //   // for operations that deal with bits rather than numbers).
        //   BuildInt32BinarySmiOperationNode<kOperation>();
        //   return;
      }
      break;
    default:
      // Fallback to generic node.
      break;
  }
  BuildGenericBinarySmiOperationNode<kOperation>();
}

template <typename CompareControlNode>
bool MaglevGraphBuilder::TryBuildCompareOperation(Operation operation,
                                                  ValueNode* left,
                                                  ValueNode* right) {
  // Don't emit the shortcut branch if the next bytecode is a merge target.
  if (IsOffsetAMergePoint(next_offset())) return false;

  interpreter::Bytecode next_bytecode = iterator_.next_bytecode();
  int true_offset;
  int false_offset;
  switch (next_bytecode) {
    case interpreter::Bytecode::kJumpIfFalse:
    case interpreter::Bytecode::kJumpIfFalseConstant:
    case interpreter::Bytecode::kJumpIfToBooleanFalse:
    case interpreter::Bytecode::kJumpIfToBooleanFalseConstant:
      // This jump must kill the accumulator, otherwise we need to
      // materialize the actual boolean value.
      if (GetOutLivenessFor(next_offset())->AccumulatorIsLive()) return false;
      // Advance the iterator past the test to the jump, skipping
      // emitting the test.
      iterator_.Advance();
      true_offset = next_offset();
      false_offset = iterator_.GetJumpTargetOffset();
      break;
    case interpreter::Bytecode::kJumpIfTrue:
    case interpreter::Bytecode::kJumpIfTrueConstant:
    case interpreter::Bytecode::kJumpIfToBooleanTrue:
    case interpreter::Bytecode::kJumpIfToBooleanTrueConstant:
      // This jump must kill the accumulator, otherwise we need to
      // materialize the actual boolean value.
      if (GetOutLivenessFor(next_offset())->AccumulatorIsLive()) return false;
      // Advance the iterator past the test to the jump, skipping
      // emitting the test.
      iterator_.Advance();
      true_offset = iterator_.GetJumpTargetOffset();
      false_offset = next_offset();
      break;
    default:
      return false;
  }

  BasicBlock* block = FinishBlock<CompareControlNode>(
      next_offset(), {left, right}, operation, &jump_targets_[true_offset],
      &jump_targets_[false_offset]);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
  return true;
}

template <Operation kOperation>
void MaglevGraphBuilder::VisitCompareOperation() {
  FeedbackNexus nexus = FeedbackNexusForOperand(1);
  switch (nexus.GetCompareOperationFeedback()) {
    case CompareOperationHint::kNone:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForCompareOperation);
      return;
    case CompareOperationHint::kSignedSmall:
      if (BinaryOperationHasInt32FastPath<kOperation>()) {
        ValueNode *left, *right;
        if (IsRegisterEqualToAccumulator(0)) {
          left = right = LoadRegisterInt32(0);
        } else {
          left = LoadRegisterInt32(0);
          right = GetAccumulatorInt32();
        }
        if (TryBuildCompareOperation<BranchIfInt32Compare>(kOperation, left,
                                                           right)) {
          return;
        }
        SetAccumulator(
            AddNewInt32BinaryOperationNode<kOperation>({left, right}));
        return;
      }
      break;
    case CompareOperationHint::kNumber:
      if (BinaryOperationHasFloat64FastPath<kOperation>()) {
        ValueNode *left, *right;
        if (IsRegisterEqualToAccumulator(0)) {
          left = right = LoadRegisterFloat64(0);
        } else {
          left = LoadRegisterFloat64(0);
          right = GetAccumulatorFloat64();
        }
        if (TryBuildCompareOperation<BranchIfFloat64Compare>(kOperation, left,
                                                             right)) {
          return;
        }
        SetAccumulator(
            AddNewFloat64BinaryOperationNode<kOperation>({left, right}));
        return;
        // } else if (BinaryOperationHasInt32FastPath<kOperation>()) {
        //   // Fall back to int32 fast path if there is one (this will be the
        //   case
        //   // for operations that deal with bits rather than numbers).
        //   BuildInt32BinaryOperationNode<kOperation>();
        //   return;
      }
      break;
    case CompareOperationHint::kInternalizedString: {
      DCHECK(kOperation == Operation::kEqual ||
             kOperation == Operation::kStrictEqual);
      ValueNode *left, *right;
      if (IsRegisterEqualToAccumulator(0)) {
        left = right = LoadRegister<CheckedInternalizedString>(0);
      } else {
        left = LoadRegister<CheckedInternalizedString>(0);
        right = GetAccumulator<CheckedInternalizedString>();
      }
      if (TryBuildCompareOperation<BranchIfReferenceCompare>(kOperation, left,
                                                             right)) {
        return;
      }
      SetAccumulator(AddNewNode<TaggedEqual>({left, right}));
      return;
    }
    case CompareOperationHint::kSymbol: {
      DCHECK(kOperation == Operation::kEqual ||
             kOperation == Operation::kStrictEqual);
      ValueNode *left, *right;
      if (IsRegisterEqualToAccumulator(0)) {
        left = right = LoadRegisterTagged(0);
        BuildCheckSymbol(left);
      } else {
        left = LoadRegisterTagged(0);
        right = GetAccumulatorTagged();
        BuildCheckSymbol(left);
        BuildCheckSymbol(right);
      }
      if (TryBuildCompareOperation<BranchIfReferenceCompare>(kOperation, left,
                                                             right)) {
        return;
      }
      SetAccumulator(AddNewNode<TaggedEqual>({left, right}));
      return;
    }
    default:
      // Fallback to generic node.
      break;
  }
  BuildGenericBinaryOperationNode<kOperation>();
}

void MaglevGraphBuilder::VisitLdar() {
  MoveNodeBetweenRegisters(iterator_.GetRegisterOperand(0),
                           interpreter::Register::virtual_accumulator());
}

void MaglevGraphBuilder::VisitLdaZero() { SetAccumulator(GetSmiConstant(0)); }
void MaglevGraphBuilder::VisitLdaSmi() {
  int constant = iterator_.GetImmediateOperand(0);
  SetAccumulator(GetSmiConstant(constant));
}
void MaglevGraphBuilder::VisitLdaUndefined() {
  SetAccumulator(GetRootConstant(RootIndex::kUndefinedValue));
}
void MaglevGraphBuilder::VisitLdaNull() {
  SetAccumulator(GetRootConstant(RootIndex::kNullValue));
}
void MaglevGraphBuilder::VisitLdaTheHole() {
  SetAccumulator(GetRootConstant(RootIndex::kTheHoleValue));
}
void MaglevGraphBuilder::VisitLdaTrue() {
  SetAccumulator(GetRootConstant(RootIndex::kTrueValue));
}
void MaglevGraphBuilder::VisitLdaFalse() {
  SetAccumulator(GetRootConstant(RootIndex::kFalseValue));
}
void MaglevGraphBuilder::VisitLdaConstant() {
  SetAccumulator(GetConstant(GetRefOperand<HeapObject>(0)));
}

void MaglevGraphBuilder::VisitLdaContextSlot() {
  ValueNode* context = LoadRegisterTagged(0);
  int slot_index = iterator_.GetIndexOperand(1);
  int depth = iterator_.GetUnsignedImmediateOperand(2);

  for (int i = 0; i < depth; ++i) {
    context = AddNewNode<LoadTaggedField>(
        {context}, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX));
  }

  SetAccumulator(AddNewNode<LoadTaggedField>(
      {context}, Context::OffsetOfElementAt(slot_index)));
}
void MaglevGraphBuilder::VisitLdaImmutableContextSlot() {
  // TODO(leszeks): Consider context specialising.
  VisitLdaContextSlot();
}
void MaglevGraphBuilder::VisitLdaCurrentContextSlot() {
  ValueNode* context = GetContext();
  int slot_index = iterator_.GetIndexOperand(0);

  SetAccumulator(AddNewNode<LoadTaggedField>(
      {context}, Context::OffsetOfElementAt(slot_index)));
}
void MaglevGraphBuilder::VisitLdaImmutableCurrentContextSlot() {
  // TODO(leszeks): Consider context specialising.
  VisitLdaCurrentContextSlot();
}

void MaglevGraphBuilder::VisitStaContextSlot() {
  ValueNode* context = LoadRegisterTagged(0);
  int slot_index = iterator_.GetIndexOperand(1);
  int depth = iterator_.GetUnsignedImmediateOperand(2);

  for (int i = 0; i < depth; ++i) {
    context = AddNewNode<LoadTaggedField>(
        {context}, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX));
  }

  AddNewNode<StoreTaggedFieldWithWriteBarrier>(
      {context, GetAccumulatorTagged()},
      Context::OffsetOfElementAt(slot_index));
}
void MaglevGraphBuilder::VisitStaCurrentContextSlot() {
  ValueNode* context = GetContext();
  int slot_index = iterator_.GetIndexOperand(0);

  AddNewNode<StoreTaggedFieldWithWriteBarrier>(
      {context, GetAccumulatorTagged()},
      Context::OffsetOfElementAt(slot_index));
}

void MaglevGraphBuilder::VisitStar() {
  MoveNodeBetweenRegisters(interpreter::Register::virtual_accumulator(),
                           iterator_.GetRegisterOperand(0));
}
#define SHORT_STAR_VISITOR(Name, ...)                                          \
  void MaglevGraphBuilder::Visit##Name() {                                     \
    MoveNodeBetweenRegisters(                                                  \
        interpreter::Register::virtual_accumulator(),                          \
        interpreter::Register::FromShortStar(interpreter::Bytecode::k##Name)); \
  }
SHORT_STAR_BYTECODE_LIST(SHORT_STAR_VISITOR)
#undef SHORT_STAR_VISITOR

void MaglevGraphBuilder::VisitMov() {
  MoveNodeBetweenRegisters(iterator_.GetRegisterOperand(0),
                           iterator_.GetRegisterOperand(1));
}

void MaglevGraphBuilder::VisitPushContext() {
  MoveNodeBetweenRegisters(interpreter::Register::current_context(),
                           iterator_.GetRegisterOperand(0));
  SetContext(GetAccumulatorTagged());
}

void MaglevGraphBuilder::VisitPopContext() {
  SetContext(LoadRegisterTagged(0));
}

void MaglevGraphBuilder::VisitTestReferenceEqual() {
  ValueNode* lhs = LoadRegisterTagged(0);
  ValueNode* rhs = GetAccumulatorTagged();
  if (lhs == rhs) {
    SetAccumulator(GetRootConstant(RootIndex::kTrueValue));
    return;
  }
  SetAccumulator(AddNewNode<TaggedEqual>({lhs, rhs}));
}

void MaglevGraphBuilder::VisitTestUndetectable() {
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<TestUndetectable>({value}));
}

void MaglevGraphBuilder::VisitTestNull() {
  ValueNode* value = GetAccumulatorTagged();
  if (RootConstant* constant = value->TryCast<RootConstant>()) {
    if (constant->index() == RootIndex::kNullValue) {
      SetAccumulator(GetRootConstant(RootIndex::kTrueValue));
    } else {
      SetAccumulator(GetRootConstant(RootIndex::kFalseValue));
    }
    return;
  }
  ValueNode* null_constant = GetRootConstant(RootIndex::kNullValue);
  SetAccumulator(AddNewNode<TaggedEqual>({value, null_constant}));
}

void MaglevGraphBuilder::VisitTestUndefined() {
  ValueNode* value = GetAccumulatorTagged();
  if (RootConstant* constant = value->TryCast<RootConstant>()) {
    if (constant->index() == RootIndex::kUndefinedValue) {
      SetAccumulator(GetRootConstant(RootIndex::kTrueValue));
    } else {
      SetAccumulator(GetRootConstant(RootIndex::kFalseValue));
    }
    return;
  }
  ValueNode* undefined_constant = GetRootConstant(RootIndex::kUndefinedValue);
  SetAccumulator(AddNewNode<TaggedEqual>({value, undefined_constant}));
}

void MaglevGraphBuilder::VisitTestTypeOf() {
  using LiteralFlag = interpreter::TestTypeOfFlags::LiteralFlag;
  // TODO(v8:7700): Add a branch version of TestTypeOf that does not need to
  // materialise the boolean value.
  LiteralFlag literal =
      interpreter::TestTypeOfFlags::Decode(GetFlag8Operand(0));
  if (literal == LiteralFlag::kOther) {
    SetAccumulator(GetRootConstant(RootIndex::kFalseValue));
    return;
  }
  ValueNode* value = GetAccumulatorTagged();
  // TODO(victorgomes): Add fast path for constants.
  SetAccumulator(AddNewNode<TestTypeOf>({value}, literal));
}

bool MaglevGraphBuilder::TryBuildPropertyCellAccess(
    const compiler::GlobalAccessFeedback& global_access_feedback) {
  // TODO(leszeks): A bunch of this is copied from
  // js-native-context-specialization.cc -- I wonder if we can unify it
  // somehow.

  if (!global_access_feedback.IsPropertyCell()) return false;
  compiler::PropertyCellRef property_cell =
      global_access_feedback.property_cell();

  if (!property_cell.Cache()) return false;

  compiler::ObjectRef property_cell_value = property_cell.value();
  if (property_cell_value.IsTheHole()) {
    // The property cell is no longer valid.
    EmitUnconditionalDeopt(
        DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
    return true;
  }

  PropertyDetails property_details = property_cell.property_details();
  PropertyCellType property_cell_type = property_details.cell_type();
  DCHECK_EQ(PropertyKind::kData, property_details.kind());

  if (!property_details.IsConfigurable() && property_details.IsReadOnly()) {
    SetAccumulator(GetConstant(property_cell_value));
    return true;
  }

  // Record a code dependency on the cell if we can benefit from the
  // additional feedback, or the global property is configurable (i.e.
  // can be deleted or reconfigured to an accessor property).
  if (property_cell_type != PropertyCellType::kMutable ||
      property_details.IsConfigurable()) {
    broker()->dependencies()->DependOnGlobalProperty(property_cell);
  }

  // Load from constant/undefined global property can be constant-folded.
  if (property_cell_type == PropertyCellType::kConstant ||
      property_cell_type == PropertyCellType::kUndefined) {
    SetAccumulator(GetConstant(property_cell_value));
    return true;
  }

  ValueNode* property_cell_node = GetConstant(property_cell.AsHeapObject());
  SetAccumulator(AddNewNode<LoadTaggedField>({property_cell_node},
                                             PropertyCell::kValueOffset));
  return true;
}

void MaglevGraphBuilder::VisitLdaGlobal() {
  // LdaGlobal <name_index> <slot>

  static const int kNameOperandIndex = 0;
  static const int kSlotOperandIndex = 1;

  compiler::NameRef name = GetRefOperand<Name>(kNameOperandIndex);
  FeedbackSlot slot = GetSlotOperand(kSlotOperandIndex);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  BuildLoadGlobal(name, feedback_source, TypeofMode::kNotInside);
}

void MaglevGraphBuilder::VisitLdaGlobalInsideTypeof() {
  // LdaGlobalInsideTypeof <name_index> <slot>

  static const int kNameOperandIndex = 0;
  static const int kSlotOperandIndex = 1;

  compiler::NameRef name = GetRefOperand<Name>(kNameOperandIndex);
  FeedbackSlot slot = GetSlotOperand(kSlotOperandIndex);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  BuildLoadGlobal(name, feedback_source, TypeofMode::kInside);
}

void MaglevGraphBuilder::VisitStaGlobal() {
  // StaGlobal <name_index> <slot>
  ValueNode* value = GetAccumulatorTagged();
  compiler::NameRef name = GetRefOperand<Name>(0);
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  // TODO(v8:7700): Add fast path.

  ValueNode* context = GetContext();
  SetAccumulator(
      AddNewNode<StoreGlobal>({context, value}, name, feedback_source));
}

void MaglevGraphBuilder::VisitLdaLookupSlot() {
  // LdaLookupSlot <name_index>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  SetAccumulator(BuildCallRuntime(Runtime::kLoadLookupSlot, {name}));
}

void MaglevGraphBuilder::VisitLdaLookupContextSlot() {
  // LdaLookupContextSlot <name_index> <feedback_slot> <depth>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  ValueNode* slot = GetSmiConstant(iterator_.GetIndexOperand(1));
  ValueNode* depth = GetSmiConstant(iterator_.GetUnsignedImmediateOperand(2));
  SetAccumulator(
      BuildCallBuiltin<Builtin::kLookupContextTrampoline>({name, depth, slot}));
}

void MaglevGraphBuilder::VisitLdaLookupGlobalSlot() {
  // LdaLookupGlobalSlot <name_index> <feedback_slot> <depth>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  ValueNode* slot = GetSmiConstant(iterator_.GetIndexOperand(1));
  ValueNode* depth = GetSmiConstant(iterator_.GetUnsignedImmediateOperand(2));
  SetAccumulator(BuildCallBuiltin<Builtin::kLookupGlobalICTrampoline>(
      {name, depth, slot}));
}

void MaglevGraphBuilder::VisitLdaLookupSlotInsideTypeof() {
  // LdaLookupSlotInsideTypeof <name_index>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  SetAccumulator(
      BuildCallRuntime(Runtime::kLoadLookupSlotInsideTypeof, {name}));
}

void MaglevGraphBuilder::VisitLdaLookupContextSlotInsideTypeof() {
  // LdaLookupContextSlotInsideTypeof <name_index> <context_slot> <depth>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  ValueNode* slot = GetSmiConstant(iterator_.GetIndexOperand(1));
  ValueNode* depth = GetSmiConstant(iterator_.GetUnsignedImmediateOperand(2));
  SetAccumulator(
      BuildCallBuiltin<Builtin::kLookupContextInsideTypeofTrampoline>(
          {name, depth, slot}));
}

void MaglevGraphBuilder::VisitLdaLookupGlobalSlotInsideTypeof() {
  // LdaLookupGlobalSlotInsideTypeof <name_index> <context_slot> <depth>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  ValueNode* slot = GetSmiConstant(iterator_.GetIndexOperand(1));
  ValueNode* depth = GetSmiConstant(iterator_.GetUnsignedImmediateOperand(2));
  SetAccumulator(
      BuildCallBuiltin<Builtin::kLookupGlobalICInsideTypeofTrampoline>(
          {name, depth, slot}));
}

namespace {
Runtime::FunctionId StaLookupSlotFunction(uint8_t sta_lookup_slot_flags) {
  using Flags = interpreter::StoreLookupSlotFlags;
  switch (Flags::GetLanguageMode(sta_lookup_slot_flags)) {
    case LanguageMode::kStrict:
      return Runtime::kStoreLookupSlot_Strict;
    case LanguageMode::kSloppy:
      if (Flags::IsLookupHoistingMode(sta_lookup_slot_flags)) {
        return Runtime::kStoreLookupSlot_SloppyHoisting;
      } else {
        return Runtime::kStoreLookupSlot_Sloppy;
      }
  }
}
}  // namespace

void MaglevGraphBuilder::VisitStaLookupSlot() {
  // StaLookupSlot <name_index> <flags>
  ValueNode* value = GetAccumulatorTagged();
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  uint32_t flags = GetFlag8Operand(1);
  SetAccumulator(BuildCallRuntime(StaLookupSlotFunction(flags), {name, value}));
}

void MaglevGraphBuilder::BuildCheckSmi(ValueNode* object) {
  NodeInfo* known_info = known_node_aspects().GetInfoFor(object);
  if (NodeInfo::IsSmi(known_info)) return;

  // TODO(leszeks): Figure out a way to also handle CheckedSmiUntag.
  AddNewNode<CheckSmi>({object});
  known_node_aspects().InsertOrUpdateNodeType(object, known_info,
                                              NodeType::kSmi);
}

void MaglevGraphBuilder::BuildCheckHeapObject(ValueNode* object) {
  NodeInfo* known_info = known_node_aspects().GetInfoFor(object);
  if (NodeInfo::IsAnyHeapObject(known_info)) return;

  AddNewNode<CheckHeapObject>({object});
  known_node_aspects().InsertOrUpdateNodeType(object, known_info,
                                              NodeType::kAnyHeapObject);
}

namespace {
CheckType GetCheckType(NodeInfo* known_info) {
  if (NodeInfo::IsAnyHeapObject(known_info)) {
    return CheckType::kOmitHeapObjectCheck;
  }
  return CheckType::kCheckHeapObject;
}
}  // namespace

void MaglevGraphBuilder::BuildCheckString(ValueNode* object) {
  NodeInfo* known_info = known_node_aspects().GetInfoFor(object);
  if (NodeInfo::IsString(known_info)) return;

  AddNewNode<CheckString>({object}, GetCheckType(known_info));
  known_node_aspects().InsertOrUpdateNodeType(object, known_info,
                                              NodeType::kString);
}

void MaglevGraphBuilder::BuildCheckSymbol(ValueNode* object) {
  NodeInfo* known_info = known_node_aspects().GetInfoFor(object);
  if (NodeInfo::IsSymbol(known_info)) return;

  AddNewNode<CheckSymbol>({object}, GetCheckType(known_info));
  known_node_aspects().InsertOrUpdateNodeType(object, known_info,
                                              NodeType::kSymbol);
}

void MaglevGraphBuilder::BuildMapCheck(ValueNode* object,
                                       const compiler::MapRef& map) {
  ZoneMap<ValueNode*, compiler::MapRef>& map_of_maps =
      map.is_stable() ? known_node_aspects().stable_maps
                      : known_node_aspects().unstable_maps;
  auto it = map_of_maps.find(object);
  if (it != map_of_maps.end()) {
    if (it->second.equals(map)) {
      // Map is already checked.
      return;
    }
    // TODO(leszeks): Insert an unconditional deopt if the known type doesn't
    // match the required type.
  }
  NodeInfo* known_info = known_node_aspects().GetInfoFor(object);
  if (map.is_migration_target()) {
    AddNewNode<CheckMapsWithMigration>({object}, map, GetCheckType(known_info));
  } else {
    AddNewNode<CheckMaps>({object}, map, GetCheckType(known_info));
  }
  map_of_maps.emplace(object, map);
  if (map.is_stable()) {
    compilation_unit_->broker()->dependencies()->DependOnStableMap(map);
  }
  known_node_aspects().InsertOrUpdateNodeType(
      object, known_info, NodeType::kHeapObjectWithKnownMap);
}

bool MaglevGraphBuilder::TryBuildMonomorphicLoad(ValueNode* receiver,
                                                 ValueNode* lookup_start_object,
                                                 const compiler::MapRef& map,
                                                 MaybeObjectHandle handler) {
  if (handler.is_null()) return false;

  if (handler->IsSmi()) {
    return TryBuildMonomorphicLoadFromSmiHandler(receiver, lookup_start_object,
                                                 map, handler->ToSmi().value());
  }
  HeapObject ho_handler;
  if (!handler->GetHeapObject(&ho_handler)) return false;

  if (ho_handler.IsCodeT()) {
    // TODO(leszeks): Call the code object directly.
    return false;
  } else if (ho_handler.IsAccessorPair()) {
    // TODO(leszeks): Call the getter directly.
    return false;
  } else {
    return TryBuildMonomorphicLoadFromLoadHandler(
        receiver, lookup_start_object, map, LoadHandler::cast(ho_handler));
  }
}

bool MaglevGraphBuilder::TryBuildMonomorphicLoadFromSmiHandler(
    ValueNode* receiver, ValueNode* lookup_start_object,
    const compiler::MapRef& map, int32_t handler) {
  // Smi handler, emit a map check and LoadField.
  LoadHandler::Kind kind = LoadHandler::KindBits::decode(handler);
  if (kind != LoadHandler::Kind::kField) return false;
  if (LoadHandler::IsWasmStructBits::decode(handler)) return false;

  BuildMapCheck(lookup_start_object, map);

  ValueNode* load_source;
  if (LoadHandler::IsInobjectBits::decode(handler)) {
    load_source = lookup_start_object;
  } else {
    // The field is in the property array, first load it from there.
    load_source = AddNewNode<LoadTaggedField>(
        {lookup_start_object}, JSReceiver::kPropertiesOrHashOffset);
  }
  int field_index = LoadHandler::FieldIndexBits::decode(handler);
  if (LoadHandler::IsDoubleBits::decode(handler)) {
    FieldIndex field = FieldIndex::ForSmiLoadHandler(*map.object(), handler);
    DescriptorArray descriptors = *map.instance_descriptors().object();
    InternalIndex index =
        descriptors.Search(field.property_index(), *map.object());
    DCHECK(index.is_found());
    DCHECK(Representation::Double().CanBeInPlaceChangedTo(
        descriptors.GetDetails(index).representation()));
    const compiler::CompilationDependency* dep =
        broker()->dependencies()->FieldRepresentationDependencyOffTheRecord(
            map, index, Representation::Double());
    broker()->dependencies()->RecordDependency(dep);

    SetAccumulator(
        AddNewNode<LoadDoubleField>({load_source}, field_index * kTaggedSize));
  } else {
    SetAccumulator(
        AddNewNode<LoadTaggedField>({load_source}, field_index * kTaggedSize));
  }
  return true;
}

bool MaglevGraphBuilder::TryBuildMonomorphicLoadFromLoadHandler(
    ValueNode* receiver, ValueNode* lookup_start_object,
    const compiler::MapRef& map, LoadHandler handler) {
  Object maybe_smi_handler = handler.smi_handler(local_isolate_);
  if (!maybe_smi_handler.IsSmi()) return false;
  int smi_handler = Smi::cast(maybe_smi_handler).value();
  LoadHandler::Kind kind = LoadHandler::KindBits::decode(smi_handler);
  bool do_access_check_on_lookup_start_object =
      LoadHandler::DoAccessCheckOnLookupStartObjectBits::decode(smi_handler);
  bool lookup_on_lookup_start_object =
      LoadHandler::LookupOnLookupStartObjectBits::decode(smi_handler);
  if (lookup_on_lookup_start_object) return false;
  if (kind != LoadHandler::Kind::kConstantFromPrototype &&
      kind != LoadHandler::Kind::kAccessorFromPrototype)
    return false;

  if (map.IsStringMap()) {
    // Check for string maps before checking if we need to do an access check.
    // Primitive strings always get the prototype from the native context
    // they're operated on, so they don't need the access check.
    BuildCheckString(lookup_start_object);
  } else if (do_access_check_on_lookup_start_object) {
    return false;
  } else {
    BuildMapCheck(lookup_start_object, map);
  }

  Object validity_cell = handler.validity_cell(local_isolate_);
  if (validity_cell.IsCell(local_isolate_)) {
    compiler::MapRef receiver_map = map;
    if (receiver_map.IsPrimitiveMap()) {
      // Perform the implicit ToObject for primitives here.
      // Implemented according to ES6 section 7.3.2 GetV (V, P).
      // Note: Keep sync'd with AccessInfoFactory::ComputePropertyAccessInfo.
      base::Optional<compiler::JSFunctionRef> constructor =
          broker()->target_native_context().GetConstructorFunction(
              receiver_map);
      receiver_map = constructor.value().initial_map(broker()->dependencies());
    }

    compiler::MapRef proto_map = receiver_map.prototype().map();
    while (proto_map.object()->prototype_validity_cell(
               local_isolate_, kRelaxedLoad) == validity_cell) {
      broker()->dependencies()->DependOnStableMap(proto_map);
      proto_map = proto_map.prototype().map();
    }
  } else {
    DCHECK_EQ(Smi::ToInt(validity_cell), Map::kPrototypeChainValid);
  }

  switch (kind) {
    case LoadHandler::Kind::kConstantFromPrototype: {
      MaybeObject value = handler.data1(local_isolate_);
      if (value.IsSmi()) {
        SetAccumulator(GetSmiConstant(value.ToSmi().value()));
      } else {
        SetAccumulator(GetConstant(MakeRefAssumeMemoryFence(
            broker(),
            broker()->CanonicalPersistentHandle(value.GetHeapObject()))));
      }
      break;
    }
    case LoadHandler::Kind::kAccessorFromPrototype: {
      MaybeObject getter = handler.data1(local_isolate_);
      compiler::ObjectRef getter_ref = MakeRefAssumeMemoryFence(
          broker(),
          broker()->CanonicalPersistentHandle(getter.GetHeapObject()));

      Call* call = CreateNewNode<Call>(Call::kFixedInputCount + 1,
                                       ConvertReceiverMode::kNotNullOrUndefined,
                                       GetConstant(getter_ref), GetContext());
      call->set_arg(0, receiver);
      SetAccumulator(AddNode(call));
      break;
    }
    default:
      UNREACHABLE();
  }
  return true;
}

bool MaglevGraphBuilder::TryBuildMonomorphicElementLoad(
    ValueNode* object, ValueNode* index, const compiler::MapRef& map,
    MaybeObjectHandle handler) {
  if (handler.is_null()) return false;

  if (handler->IsSmi()) {
    return TryBuildMonomorphicElementLoadFromSmiHandler(
        object, index, map, handler->ToSmi().value());
  }
  return false;
}

bool MaglevGraphBuilder::TryBuildMonomorphicElementLoadFromSmiHandler(
    ValueNode* object, ValueNode* index, const compiler::MapRef& map,
    int32_t handler) {
  LoadHandler::Kind kind = LoadHandler::KindBits::decode(handler);

  switch (kind) {
    case LoadHandler::Kind::kElement: {
      if (LoadHandler::AllowOutOfBoundsBits::decode(handler)) {
        return false;
      }
      ElementsKind elements_kind =
          LoadHandler::ElementsKindBits::decode(handler);
      if (!IsFastElementsKind(elements_kind)) return false;

      // TODO(leszeks): Handle holey elements.
      if (IsHoleyElementsKind(elements_kind)) return false;
      DCHECK(!LoadHandler::ConvertHoleBits::decode(handler));

      BuildMapCheck(object, map);
      BuildCheckSmi(index);

      if (LoadHandler::IsJsArrayBits::decode(handler)) {
        DCHECK(map.IsJSArrayMap());
        AddNewNode<CheckJSArrayBounds>({object, index});
      } else {
        DCHECK(!map.IsJSArrayMap());
        DCHECK(map.IsJSObjectMap());
        AddNewNode<CheckJSObjectElementsBounds>({object, index});
      }
      if (elements_kind == ElementsKind::PACKED_DOUBLE_ELEMENTS) {
        SetAccumulator(AddNewNode<LoadDoubleElement>({object, index}));
      } else {
        DCHECK(!IsDoubleElementsKind(elements_kind));
        SetAccumulator(AddNewNode<LoadTaggedElement>({object, index}));
      }
      return true;
    }
    default:
      return false;
  }
}

void MaglevGraphBuilder::VisitGetNamedProperty() {
  // GetNamedProperty <object> <name_index> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  compiler::NameRef name = GetRefOperand<Name>(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(feedback_source,
                                             compiler::AccessMode::kLoad, name);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
      return;

    case compiler::ProcessedFeedback::kNamedAccess: {
      const compiler::NamedAccessFeedback& named_feedback =
          processed_feedback.AsNamedAccess();
      if (named_feedback.maps().size() != 1) break;
      compiler::MapRef map = named_feedback.maps()[0];

      // Monomorphic load, check the handler.
      // TODO(leszeks): Make GetFeedbackForPropertyAccess read the handler.
      MaybeObjectHandle handler =
          FeedbackNexusForSlot(slot).FindHandlerForMap(map.object());

      if (TryBuildMonomorphicLoad(object, object, map, handler)) return;
    } break;

    default:
      break;
  }

  // Create a generic load in the fallthrough.
  ValueNode* context = GetContext();
  SetAccumulator(
      AddNewNode<LoadNamedGeneric>({context, object}, name, feedback_source));
}

void MaglevGraphBuilder::VisitGetNamedPropertyFromSuper() {
  // GetNamedPropertyFromSuper <receiver> <name_index> <slot>
  ValueNode* receiver = LoadRegisterTagged(0);
  ValueNode* home_object = GetAccumulatorTagged();
  compiler::NameRef name = GetRefOperand<Name>(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  // {home_object} is guaranteed to be a HeapObject.
  ValueNode* home_object_map =
      AddNewNode<LoadTaggedField>({home_object}, HeapObject::kMapOffset);
  ValueNode* lookup_start_object =
      AddNewNode<LoadTaggedField>({home_object_map}, Map::kPrototypeOffset);

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(feedback_source,
                                             compiler::AccessMode::kLoad, name);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
      return;

    case compiler::ProcessedFeedback::kNamedAccess: {
      const compiler::NamedAccessFeedback& named_feedback =
          processed_feedback.AsNamedAccess();
      if (named_feedback.maps().size() != 1) break;
      compiler::MapRef map = named_feedback.maps()[0];

      // Monomorphic load, check the handler.
      // TODO(leszeks): Make GetFeedbackForPropertyAccess read the handler.
      MaybeObjectHandle handler =
          FeedbackNexusForSlot(slot).FindHandlerForMap(map.object());

      if (TryBuildMonomorphicLoad(receiver, lookup_start_object, map, handler))
        return;
    } break;

    default:
      break;
  }

  // Create a generic load.
  ValueNode* context = GetContext();
  SetAccumulator(AddNewNode<LoadNamedFromSuperGeneric>(
      {context, receiver, lookup_start_object}, name, feedback_source));
}

void MaglevGraphBuilder::VisitGetKeyedProperty() {
  // GetKeyedProperty <object> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  // TODO(leszeks): We don't need to tag the key if it's an Int32 and a simple
  // monomorphic element load.
  ValueNode* key = GetAccumulatorTagged();
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(
          feedback_source, compiler::AccessMode::kLoad, base::nullopt);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess);
      return;

    case compiler::ProcessedFeedback::kElementAccess: {
      const compiler::ElementAccessFeedback& element_feedback =
          processed_feedback.AsElementAccess();
      if (element_feedback.transition_groups().size() != 1) break;
      if (element_feedback.transition_groups()[0].size() != 1) break;
      compiler::MapRef map = MakeRefAssumeMemoryFence(
          broker(), element_feedback.transition_groups()[0].front());

      // Monomorphic load, check the handler.
      // TODO(leszeks): Make GetFeedbackForPropertyAccess read the handler.
      MaybeObjectHandle handler =
          FeedbackNexusForSlot(slot).FindHandlerForMap(map.object());

      if (TryBuildMonomorphicElementLoad(object, key, map, handler)) return;
      break;
    }

    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  SetAccumulator(
      AddNewNode<GetKeyedGeneric>({context, object, key}, feedback_source));
}

void MaglevGraphBuilder::VisitLdaModuleVariable() {
  // LdaModuleVariable <cell_index> <depth>
  int cell_index = iterator_.GetImmediateOperand(0);
  int depth = iterator_.GetUnsignedImmediateOperand(1);

  ValueNode* context = GetContext();
  for (int i = 0; i < depth; i++) {
    context = AddNewNode<LoadTaggedField>(
        {context}, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX));
  }
  ValueNode* module = AddNewNode<LoadTaggedField>(
      {context}, Context::OffsetOfElementAt(Context::EXTENSION_INDEX));
  ValueNode* exports_or_imports;
  if (cell_index > 0) {
    exports_or_imports = AddNewNode<LoadTaggedField>(
        {module}, SourceTextModule::kRegularExportsOffset);
    // The actual array index is (cell_index - 1).
    cell_index -= 1;
  } else {
    exports_or_imports = AddNewNode<LoadTaggedField>(
        {module}, SourceTextModule::kRegularImportsOffset);
    // The actual array index is (-cell_index - 1).
    cell_index = -cell_index - 1;
  }
  ValueNode* cell = LoadFixedArrayElement(exports_or_imports, cell_index);
  SetAccumulator(AddNewNode<LoadTaggedField>({cell}, Cell::kValueOffset));
}

void MaglevGraphBuilder::VisitStaModuleVariable() {
  // StaModuleVariable <cell_index> <depth>
  int cell_index = iterator_.GetImmediateOperand(0);
  if (V8_UNLIKELY(cell_index < 0)) {
    BuildCallRuntime(Runtime::kAbort,
                     {GetSmiConstant(static_cast<int>(
                         AbortReason::kUnsupportedModuleOperation))});
    return;
  }
  ValueNode* context = GetContext();
  int depth = iterator_.GetUnsignedImmediateOperand(1);
  for (int i = 0; i < depth; i++) {
    context = AddNewNode<LoadTaggedField>(
        {context}, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX));
  }
  ValueNode* module = AddNewNode<LoadTaggedField>(
      {context}, Context::OffsetOfElementAt(Context::EXTENSION_INDEX));
  ValueNode* exports = AddNewNode<LoadTaggedField>(
      {module}, SourceTextModule::kRegularExportsOffset);
  // The actual array index is (cell_index - 1).
  cell_index -= 1;
  ValueNode* cell = LoadFixedArrayElement(exports, cell_index);
  AddNewNode<StoreTaggedFieldWithWriteBarrier>({cell, GetAccumulatorTagged()},
                                               Cell::kValueOffset);
}

bool MaglevGraphBuilder::TryBuildMonomorphicStoreFromSmiHandler(
    ValueNode* object, const compiler::MapRef& map, int32_t handler) {
  StoreHandler::Kind kind = StoreHandler::KindBits::decode(handler);
  if (kind != StoreHandler::Kind::kField) return false;

  Representation::Kind representation =
      StoreHandler::RepresentationBits::decode(handler);
  if (representation == Representation::kDouble) return false;

  InternalIndex descriptor_idx(StoreHandler::DescriptorBits::decode(handler));
  PropertyDetails property_details =
      map.instance_descriptors().GetPropertyDetails(descriptor_idx);

  // TODO(leszeks): Allow a fast path which checks for equality with the current
  // value.
  if (property_details.constness() == PropertyConstness::kConst) return false;

  BuildMapCheck(object, map);

  ValueNode* store_target;
  if (StoreHandler::IsInobjectBits::decode(handler)) {
    store_target = object;
  } else {
    // The field is in the property array, first Store it from there.
    store_target = AddNewNode<LoadTaggedField>(
        {object}, JSReceiver::kPropertiesOrHashOffset);
  }

  int field_index = StoreHandler::FieldIndexBits::decode(handler);
  int offset = field_index * kTaggedSize;

  ValueNode* value = GetAccumulatorTagged();
  if (representation == Representation::kSmi) {
    BuildCheckSmi(value);
    AddNewNode<StoreTaggedFieldNoWriteBarrier>({store_target, value}, offset);
    return true;
  }

  if (representation == Representation::kHeapObject) {
    FieldType descriptors_field_type =
        map.instance_descriptors().object()->GetFieldType(descriptor_idx);
    if (descriptors_field_type.IsNone()) {
      // Store is not safe if the field type was cleared. Since we check this
      // late, we'll emit a useless map check and maybe property store load, but
      // that's fine, this case should be rare.
      return false;
    }

    // Emit a map check for the field type, if needed, otherwise just a
    // HeapObject check.
    if (descriptors_field_type.IsClass()) {
      // Check that the value matches the expected field type.
      base::Optional<compiler::MapRef> maybe_field_map =
          TryMakeRef(broker(), descriptors_field_type.AsClass());
      if (!maybe_field_map.has_value()) return false;

      BuildMapCheck(value, *maybe_field_map);
    } else {
      BuildCheckHeapObject(value);
    }
  }
  AddNewNode<StoreTaggedFieldWithWriteBarrier>({store_target, value}, offset);
  return true;
}

bool MaglevGraphBuilder::TryBuildMonomorphicStore(ValueNode* object,
                                                  const compiler::MapRef& map,
                                                  MaybeObjectHandle handler) {
  if (handler.is_null()) return false;

  if (handler->IsSmi()) {
    return TryBuildMonomorphicStoreFromSmiHandler(object, map,
                                                  handler->ToSmi().value());
  }
  // TODO(leszeks): If we add non-Smi paths here, make sure to differentiate
  // between Define and Set.

  return false;
}

void MaglevGraphBuilder::BuildLoadGlobal(
    compiler::NameRef name, compiler::FeedbackSource& feedback_source,
    TypeofMode typeof_mode) {
  const compiler::ProcessedFeedback& access_feedback =
      broker()->GetFeedbackForGlobalAccess(feedback_source);

  if (access_feedback.IsInsufficient()) {
    EmitUnconditionalDeopt(
        DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
    return;
  }

  const compiler::GlobalAccessFeedback& global_access_feedback =
      access_feedback.AsGlobalAccess();

  if (TryBuildPropertyCellAccess(global_access_feedback)) return;

  // TODO(leszeks): Handle the IsScriptContextSlot case.

  ValueNode* context = GetContext();
  SetAccumulator(
      AddNewNode<LoadGlobal>({context}, name, feedback_source, typeof_mode));
}

void MaglevGraphBuilder::VisitSetNamedProperty() {
  // SetNamedProperty <object> <name_index> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  compiler::NameRef name = GetRefOperand<Name>(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(
          feedback_source, compiler::AccessMode::kStore, name);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
      return;

    case compiler::ProcessedFeedback::kNamedAccess: {
      const compiler::NamedAccessFeedback& named_feedback =
          processed_feedback.AsNamedAccess();
      if (named_feedback.maps().size() != 1) break;
      compiler::MapRef map = named_feedback.maps()[0];

      // Monomorphic store, check the handler.
      // TODO(leszeks): Make GetFeedbackForPropertyAccess read the handler.
      MaybeObjectHandle handler =
          FeedbackNexusForSlot(slot).FindHandlerForMap(map.object());

      if (TryBuildMonomorphicStore(object, map, handler)) return;
    } break;

    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<SetNamedGeneric>({context, object, value}, name,
                                             feedback_source));
}

void MaglevGraphBuilder::VisitDefineNamedOwnProperty() {
  // DefineNamedOwnProperty <object> <name_index> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  compiler::NameRef name = GetRefOperand<Name>(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(
          feedback_source, compiler::AccessMode::kStore, name);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
      return;

    case compiler::ProcessedFeedback::kNamedAccess: {
      const compiler::NamedAccessFeedback& named_feedback =
          processed_feedback.AsNamedAccess();
      if (named_feedback.maps().size() != 1) break;
      compiler::MapRef map = named_feedback.maps()[0];

      // Monomorphic store, check the handler.
      // TODO(leszeks): Make GetFeedbackForPropertyAccess read the handler.
      MaybeObjectHandle handler =
          FeedbackNexusForSlot(slot).FindHandlerForMap(map.object());

      if (TryBuildMonomorphicStore(object, map, handler)) return;
    } break;

    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<DefineNamedOwnGeneric>({context, object, value},
                                                   name, feedback_source));
}

void MaglevGraphBuilder::VisitSetKeyedProperty() {
  // SetKeyedProperty <object> <key> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* key = LoadRegisterTagged(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  // TODO(victorgomes): Add monomorphic fast path.

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<SetKeyedGeneric>({context, object, key, value},
                                             feedback_source));
}

void MaglevGraphBuilder::VisitDefineKeyedOwnProperty() {
  // DefineKeyedOwnProperty <object> <key> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* key = LoadRegisterTagged(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  // TODO(victorgomes): Add monomorphic fast path.

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<DefineKeyedOwnGeneric>(
      {context, object, key, value}, feedback_source));
}

void MaglevGraphBuilder::VisitStaInArrayLiteral() {
  // StaInArrayLiteral <object> <name_reg> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* name = LoadRegisterTagged(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(
          feedback_source, compiler::AccessMode::kStoreInLiteral,
          base::nullopt);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess);
      return;

    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<StoreInArrayLiteralGeneric>(
      {context, object, name, value}, feedback_source));
}

void MaglevGraphBuilder::VisitDefineKeyedOwnPropertyInLiteral() {
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* name = LoadRegisterTagged(1);
  ValueNode* value = GetAccumulatorTagged();
  ValueNode* flags = GetSmiConstant(GetFlag8Operand(2));
  ValueNode* slot = GetSmiConstant(GetSlotOperand(3).ToInt());
  ValueNode* feedback_vector = GetConstant(feedback());
  SetAccumulator(
      BuildCallRuntime(Runtime::kDefineKeyedOwnPropertyInLiteral,
                       {object, name, value, flags, feedback_vector, slot}));
}

void MaglevGraphBuilder::VisitCollectTypeProfile() {
  ValueNode* position = GetSmiConstant(GetFlag8Operand(0));
  ValueNode* value = GetAccumulatorTagged();
  ValueNode* feedback_vector = GetConstant(feedback());
  SetAccumulator(BuildCallRuntime(Runtime::kCollectTypeProfile,
                                  {position, value, feedback_vector}));
}

void MaglevGraphBuilder::VisitAdd() { VisitBinaryOperation<Operation::kAdd>(); }
void MaglevGraphBuilder::VisitSub() {
  VisitBinaryOperation<Operation::kSubtract>();
}
void MaglevGraphBuilder::VisitMul() {
  VisitBinaryOperation<Operation::kMultiply>();
}
void MaglevGraphBuilder::VisitDiv() {
  VisitBinaryOperation<Operation::kDivide>();
}
void MaglevGraphBuilder::VisitMod() {
  VisitBinaryOperation<Operation::kModulus>();
}
void MaglevGraphBuilder::VisitExp() {
  VisitBinaryOperation<Operation::kExponentiate>();
}
void MaglevGraphBuilder::VisitBitwiseOr() {
  VisitBinaryOperation<Operation::kBitwiseOr>();
}
void MaglevGraphBuilder::VisitBitwiseXor() {
  VisitBinaryOperation<Operation::kBitwiseXor>();
}
void MaglevGraphBuilder::VisitBitwiseAnd() {
  VisitBinaryOperation<Operation::kBitwiseAnd>();
}
void MaglevGraphBuilder::VisitShiftLeft() {
  VisitBinaryOperation<Operation::kShiftLeft>();
}
void MaglevGraphBuilder::VisitShiftRight() {
  VisitBinaryOperation<Operation::kShiftRight>();
}
void MaglevGraphBuilder::VisitShiftRightLogical() {
  VisitBinaryOperation<Operation::kShiftRightLogical>();
}

void MaglevGraphBuilder::VisitAddSmi() {
  VisitBinarySmiOperation<Operation::kAdd>();
}
void MaglevGraphBuilder::VisitSubSmi() {
  VisitBinarySmiOperation<Operation::kSubtract>();
}
void MaglevGraphBuilder::VisitMulSmi() {
  VisitBinarySmiOperation<Operation::kMultiply>();
}
void MaglevGraphBuilder::VisitDivSmi() {
  VisitBinarySmiOperation<Operation::kDivide>();
}
void MaglevGraphBuilder::VisitModSmi() {
  VisitBinarySmiOperation<Operation::kModulus>();
}
void MaglevGraphBuilder::VisitExpSmi() {
  VisitBinarySmiOperation<Operation::kExponentiate>();
}
void MaglevGraphBuilder::VisitBitwiseOrSmi() {
  VisitBinarySmiOperation<Operation::kBitwiseOr>();
}
void MaglevGraphBuilder::VisitBitwiseXorSmi() {
  VisitBinarySmiOperation<Operation::kBitwiseXor>();
}
void MaglevGraphBuilder::VisitBitwiseAndSmi() {
  VisitBinarySmiOperation<Operation::kBitwiseAnd>();
}
void MaglevGraphBuilder::VisitShiftLeftSmi() {
  VisitBinarySmiOperation<Operation::kShiftLeft>();
}
void MaglevGraphBuilder::VisitShiftRightSmi() {
  VisitBinarySmiOperation<Operation::kShiftRight>();
}
void MaglevGraphBuilder::VisitShiftRightLogicalSmi() {
  VisitBinarySmiOperation<Operation::kShiftRightLogical>();
}

void MaglevGraphBuilder::VisitInc() {
  VisitUnaryOperation<Operation::kIncrement>();
}
void MaglevGraphBuilder::VisitDec() {
  VisitUnaryOperation<Operation::kDecrement>();
}
void MaglevGraphBuilder::VisitNegate() {
  VisitUnaryOperation<Operation::kNegate>();
}
void MaglevGraphBuilder::VisitBitwiseNot() {
  VisitUnaryOperation<Operation::kBitwiseNot>();
}

void MaglevGraphBuilder::VisitToBooleanLogicalNot() {
  ValueNode* value = GetAccumulatorTagged();
  switch (value->opcode()) {
#define CASE(Name)                                                            \
  case Opcode::k##Name: {                                                     \
    SetAccumulator(                                                           \
        GetBooleanConstant(value->Cast<Name>()->ToBoolean(local_isolate()))); \
    break;                                                                    \
  }
    CONSTANT_VALUE_NODE_LIST(CASE)
#undef CASE
    default:
      SetAccumulator(AddNewNode<ToBooleanLogicalNot>({value}));
      break;
  }
}

void MaglevGraphBuilder::VisitLogicalNot() {
  // Invariant: accumulator must already be a boolean value.
  ValueNode* value = GetAccumulatorTagged();
  switch (value->opcode()) {
#define CASE(Name)                                                            \
  case Opcode::k##Name: {                                                     \
    SetAccumulator(                                                           \
        GetBooleanConstant(value->Cast<Name>()->ToBoolean(local_isolate()))); \
    break;                                                                    \
  }
    CONSTANT_VALUE_NODE_LIST(CASE)
#undef CASE
    default:
      SetAccumulator(AddNewNode<LogicalNot>({value}));
      break;
  }
}

void MaglevGraphBuilder::VisitTypeOf() {
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(BuildCallBuiltin<Builtin::kTypeof>({value}));
}

void MaglevGraphBuilder::VisitDeletePropertyStrict() {
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* key = GetAccumulatorTagged();
  ValueNode* context = GetContext();
  SetAccumulator(AddNewNode<DeleteProperty>({context, object, key},
                                            LanguageMode::kStrict));
}

void MaglevGraphBuilder::VisitDeletePropertySloppy() {
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* key = GetAccumulatorTagged();
  ValueNode* context = GetContext();
  SetAccumulator(AddNewNode<DeleteProperty>({context, object, key},
                                            LanguageMode::kSloppy));
}

void MaglevGraphBuilder::VisitGetSuperConstructor() {
  ValueNode* active_function = GetAccumulatorTagged();
  ValueNode* map =
      AddNewNode<LoadTaggedField>({active_function}, HeapObject::kMapOffset);
  ValueNode* map_proto =
      AddNewNode<LoadTaggedField>({map}, Map::kPrototypeOffset);
  StoreRegister(iterator_.GetRegisterOperand(0), map_proto);
}

void MaglevGraphBuilder::VisitFindNonDefaultConstructor() {
  // TODO(v8:13091): Implement.
  CHECK(false);
}

void MaglevGraphBuilder::InlineCallFromRegisters(
    int argc_count, ConvertReceiverMode receiver_mode,
    compiler::JSFunctionRef function) {
  // The undefined constant node has to be created before the inner graph is
  // created.
  RootConstant* undefined_constant;
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    undefined_constant = GetRootConstant(RootIndex::kUndefinedValue);
  }

  // Create a new compilation unit and graph builder for the inlined
  // function.
  MaglevCompilationUnit* inner_unit =
      MaglevCompilationUnit::NewInner(zone(), compilation_unit_, function);
  MaglevGraphBuilder inner_graph_builder(local_isolate_, inner_unit, graph_,
                                         this);

  // Finish the current block with a jump to the inlined function.
  BasicBlockRef start_ref, end_ref;
  BasicBlock* block = CreateBlock<JumpToInlined>({}, &start_ref, inner_unit);
  ResolveJumpsToBlockAtOffset(block, block_offset_);

  // Manually create the prologue of the inner function graph, so that we
  // can manually set up the arguments.
  inner_graph_builder.StartPrologue();

  int arg_index = 0;
  int reg_count;
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    reg_count = argc_count;
    if (function.shared().language_mode() == LanguageMode::kSloppy) {
      // TODO(leszeks): Store the global proxy somehow.
      inner_graph_builder.SetArgument(arg_index++, undefined_constant);
    } else {
      inner_graph_builder.SetArgument(arg_index++, undefined_constant);
    }
  } else {
    reg_count = argc_count + 1;
  }
  for (int i = 0; i < reg_count && i < inner_unit->parameter_count(); i++) {
    inner_graph_builder.SetArgument(arg_index++, LoadRegisterTagged(i + 1));
  }
  for (; arg_index < inner_unit->parameter_count(); arg_index++) {
    inner_graph_builder.SetArgument(arg_index, undefined_constant);
  }
  // TODO(leszeks): Also correctly set up the closure and context slots, instead
  // of using InitialValue.
  inner_graph_builder.BuildRegisterFrameInitialization();
  inner_graph_builder.BuildMergeStates();
  BasicBlock* inlined_prologue = inner_graph_builder.EndPrologue();

  // Set the entry JumpToInlined to jump to the prologue block.
  // TODO(leszeks): Passing start_ref to JumpToInlined creates a two-element
  // linked list of refs. Consider adding a helper to explicitly set the target
  // instead.
  start_ref.SetToBlockAndReturnNext(inlined_prologue)
      ->SetToBlockAndReturnNext(inlined_prologue);

  // Build the inlined function body.
  inner_graph_builder.BuildBody();

  // All returns in the inlined body jump to a merge point one past the
  // bytecode length (i.e. at offset bytecode.length()). Create a block at
  // this fake offset and have it jump out of the inlined function, into a new
  // block that we create which resumes execution of the outer function.
  // TODO(leszeks): Wrap this up in a helper.
  DCHECK_NULL(inner_graph_builder.current_block_);
  inner_graph_builder.ProcessMergePoint(
      inner_graph_builder.inline_exit_offset());
  inner_graph_builder.StartNewBlock(inner_graph_builder.inline_exit_offset());
  BasicBlock* end_block =
      inner_graph_builder.CreateBlock<JumpFromInlined>({}, &end_ref);
  inner_graph_builder.ResolveJumpsToBlockAtOffset(
      end_block, inner_graph_builder.inline_exit_offset());

  // Pull the returned accumulator value out of the inlined function's final
  // merged return state.
  current_interpreter_frame_.set_accumulator(
      inner_graph_builder.current_interpreter_frame_.accumulator());

  // Create a new block at our current offset, and resume execution. Do this
  // manually to avoid trying to resolve any merges to this offset, which will
  // have already been processed on entry to this visitor.
  current_block_ = zone()->New<BasicBlock>(MergePointInterpreterFrameState::New(
      *compilation_unit_, current_interpreter_frame_,
      iterator_.current_offset(), 1, block, GetInLiveness()));
  block_offset_ = iterator_.current_offset();
  // Set the exit JumpFromInlined to jump to this resume block.
  // TODO(leszeks): Passing start_ref to JumpFromInlined creates a two-element
  // linked list of refs. Consider adding a helper to explicitly set the target
  // instead.
  end_ref.SetToBlockAndReturnNext(current_block_)
      ->SetToBlockAndReturnNext(current_block_);
}

// TODO(v8:7700): Read feedback and implement inlining
void MaglevGraphBuilder::BuildCallFromRegisterList(
    ConvertReceiverMode receiver_mode) {
  ValueNode* function = LoadRegisterTagged(0);

  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();

  size_t input_count = args.register_count() + Call::kFixedInputCount;
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    input_count++;
  }

  Call* call =
      CreateNewNode<Call>(input_count, receiver_mode, function, context);
  int arg_index = 0;
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    call->set_arg(arg_index++, GetRootConstant(RootIndex::kUndefinedValue));
  }
  for (int i = 0; i < args.register_count(); ++i) {
    call->set_arg(arg_index++, GetTaggedValue(args[i]));
  }

  SetAccumulator(AddNode(call));
}

void MaglevGraphBuilder::BuildCallFromRegisters(
    int argc_count, ConvertReceiverMode receiver_mode) {
  // Indices and counts of operands on the bytecode.
  const int kFirstArgumentOperandIndex = 1;
  const int kReceiverOperandCount =
      (receiver_mode == ConvertReceiverMode::kNullOrUndefined) ? 0 : 1;
  const int kReceiverAndArgOperandCount = kReceiverOperandCount + argc_count;
  const int kSlotOperandIndex =
      kFirstArgumentOperandIndex + kReceiverAndArgOperandCount;

  DCHECK_LE(argc_count, 2);
  ValueNode* function = LoadRegisterTagged(0);
  ValueNode* context = GetContext();
  FeedbackSlot slot = GetSlotOperand(kSlotOperandIndex);

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForCall(compiler::FeedbackSource(feedback(), slot));
  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForCall);
      return;

    case compiler::ProcessedFeedback::kCall: {
      if (!FLAG_maglev_inlining) break;

      const compiler::CallFeedback& call_feedback = processed_feedback.AsCall();
      CallFeedbackContent content = call_feedback.call_feedback_content();
      if (content != CallFeedbackContent::kTarget) break;

      base::Optional<compiler::HeapObjectRef> maybe_target =
          call_feedback.target();
      if (!maybe_target.has_value()) break;

      compiler::HeapObjectRef target = maybe_target.value();
      if (!target.IsJSFunction()) break;

      compiler::JSFunctionRef function = target.AsJSFunction();
      base::Optional<compiler::FeedbackVectorRef> maybe_feedback_vector =
          function.feedback_vector(broker()->dependencies());
      if (!maybe_feedback_vector.has_value()) break;

      return InlineCallFromRegisters(argc_count, receiver_mode, function);
    }

    default:
      break;
  }
  // On fallthrough, create a generic call.

  int argc_count_with_recv = argc_count + 1;
  size_t input_count = argc_count_with_recv + Call::kFixedInputCount;

  int arg_index = 0;
  int reg_count = argc_count_with_recv;
  Call* call =
      CreateNewNode<Call>(input_count, receiver_mode, function, context);
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    reg_count = argc_count;
    call->set_arg(arg_index++, GetRootConstant(RootIndex::kUndefinedValue));
  }
  for (int i = 0; i < reg_count; i++) {
    call->set_arg(arg_index++, LoadRegisterTagged(i + 1));
  }

  SetAccumulator(AddNode(call));
}

void MaglevGraphBuilder::VisitCallAnyReceiver() {
  BuildCallFromRegisterList(ConvertReceiverMode::kAny);
}
void MaglevGraphBuilder::VisitCallProperty() {
  BuildCallFromRegisterList(ConvertReceiverMode::kNotNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallProperty0() {
  BuildCallFromRegisters(0, ConvertReceiverMode::kNotNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallProperty1() {
  BuildCallFromRegisters(1, ConvertReceiverMode::kNotNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallProperty2() {
  BuildCallFromRegisters(2, ConvertReceiverMode::kNotNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallUndefinedReceiver() {
  BuildCallFromRegisterList(ConvertReceiverMode::kNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallUndefinedReceiver0() {
  BuildCallFromRegisters(0, ConvertReceiverMode::kNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallUndefinedReceiver1() {
  BuildCallFromRegisters(1, ConvertReceiverMode::kNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallUndefinedReceiver2() {
  BuildCallFromRegisters(2, ConvertReceiverMode::kNullOrUndefined);
}

void MaglevGraphBuilder::VisitCallWithSpread() {
  ValueNode* function = LoadRegisterTagged(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();

  size_t input_count = args.register_count() + CallWithSpread::kFixedInputCount;
  CallWithSpread* call =
      CreateNewNode<CallWithSpread>(input_count, function, context);
  for (int i = 0; i < args.register_count(); ++i) {
    call->set_arg(i, GetTaggedValue(args[i]));
  }

  SetAccumulator(AddNode(call));
}

void MaglevGraphBuilder::VisitCallRuntime() {
  Runtime::FunctionId function_id = iterator_.GetRuntimeIdOperand(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();

  size_t input_count = args.register_count() + CallRuntime::kFixedInputCount;
  CallRuntime* call_runtime =
      CreateNewNode<CallRuntime>(input_count, function_id, context);
  for (int i = 0; i < args.register_count(); ++i) {
    call_runtime->set_arg(i, GetTaggedValue(args[i]));
  }

  SetAccumulator(AddNode(call_runtime));
}

void MaglevGraphBuilder::VisitCallJSRuntime() {
  // Get the function to call from the native context.
  compiler::NativeContextRef native_context = broker()->target_native_context();
  ValueNode* context = GetConstant(native_context);
  uint32_t slot = iterator_.GetNativeContextIndexOperand(0);
  ValueNode* callee = AddNewNode<LoadTaggedField>(
      {context}, NativeContext::OffsetOfElementAt(slot));
  // Call the function.
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  int kTheReceiver = 1;
  size_t input_count =
      args.register_count() + Call::kFixedInputCount + kTheReceiver;

  Call* call = CreateNewNode<Call>(
      input_count, ConvertReceiverMode::kNullOrUndefined, callee, GetContext());
  int arg_index = 0;
  call->set_arg(arg_index++, GetRootConstant(RootIndex::kUndefinedValue));
  for (int i = 0; i < args.register_count(); ++i) {
    call->set_arg(arg_index++, GetTaggedValue(args[i]));
  }
  SetAccumulator(AddNode(call));
}

void MaglevGraphBuilder::VisitCallRuntimeForPair() {
  Runtime::FunctionId function_id = iterator_.GetRuntimeIdOperand(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();

  size_t input_count = args.register_count() + CallRuntime::kFixedInputCount;
  CallRuntime* call_runtime =
      AddNewNode<CallRuntime>(input_count, function_id, context);
  for (int i = 0; i < args.register_count(); ++i) {
    call_runtime->set_arg(i, GetTaggedValue(args[i]));
  }
  auto result = iterator_.GetRegisterPairOperand(3);
  StoreRegisterPair(result, call_runtime);
}

void MaglevGraphBuilder::VisitInvokeIntrinsic() {
  // InvokeIntrinsic <function_id> <first_arg> <arg_count>
  Runtime::FunctionId intrinsic_id = iterator_.GetIntrinsicIdOperand(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  switch (intrinsic_id) {
#define CASE(Name, _, arg_count)                                         \
  case Runtime::kInline##Name:                                           \
    DCHECK_IMPLIES(arg_count != -1, arg_count == args.register_count()); \
    VisitIntrinsic##Name(args);                                          \
    break;
    INTRINSICS_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

void MaglevGraphBuilder::VisitIntrinsicCopyDataProperties(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kCopyDataProperties>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::
    VisitIntrinsicCopyDataPropertiesWithExcludedPropertiesOnStack(
        interpreter::RegisterList args) {
  SmiConstant* excluded_property_count =
      GetSmiConstant(args.register_count() - 1);
  int kContext = 1;
  int kExcludedPropertyCount = 1;
  CallBuiltin* call_builtin = CreateNewNode<CallBuiltin>(
      args.register_count() + kContext + kExcludedPropertyCount,
      Builtin::kCopyDataPropertiesWithExcludedProperties, GetContext());
  int arg_index = 0;
  call_builtin->set_arg(arg_index++, GetTaggedValue(args[0]));
  call_builtin->set_arg(arg_index++, excluded_property_count);
  for (int i = 1; i < args.register_count(); i++) {
    call_builtin->set_arg(arg_index++, GetTaggedValue(args[i]));
  }
  SetAccumulator(AddNode(call_builtin));
}

void MaglevGraphBuilder::VisitIntrinsicCreateIterResultObject(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kCreateIterResultObject>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicCreateAsyncFromSyncIterator(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 1);
  SetAccumulator(
      BuildCallBuiltin<Builtin::kCreateAsyncFromSyncIteratorBaseline>(
          {GetTaggedValue(args[0])}));
}

void MaglevGraphBuilder::VisitIntrinsicCreateJSGeneratorObject(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kCreateGeneratorObject>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicGeneratorGetResumeMode(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 1);
  ValueNode* generator = GetTaggedValue(args[0]);
  SetAccumulator(AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kResumeModeOffset));
}

void MaglevGraphBuilder::VisitIntrinsicGeneratorClose(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 1);
  ValueNode* generator = GetTaggedValue(args[0]);
  ValueNode* value = GetSmiConstant(JSGeneratorObject::kGeneratorClosed);
  AddNewNode<StoreTaggedFieldNoWriteBarrier>(
      {generator, value}, JSGeneratorObject::kContinuationOffset);
  SetAccumulator(GetRootConstant(RootIndex::kUndefinedValue));
}

void MaglevGraphBuilder::VisitIntrinsicGetImportMetaObject(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 0);
  SetAccumulator(BuildCallRuntime(Runtime::kGetImportMetaObject, {}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncFunctionAwaitCaught(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncFunctionAwaitCaught>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncFunctionAwaitUncaught(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncFunctionAwaitUncaught>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncFunctionEnter(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncFunctionEnter>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncFunctionReject(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncFunctionReject>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncFunctionResolve(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncFunctionResolve>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorAwaitCaught(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorAwaitCaught>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorAwaitUncaught(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorAwaitUncaught>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorReject(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorReject>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorResolve(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 3);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorResolve>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1]),
       GetTaggedValue(args[2])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorYield(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 3);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorYield>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1]),
       GetTaggedValue(args[2])}));
}

void MaglevGraphBuilder::VisitConstruct() {
  ValueNode* new_target = GetAccumulatorTagged();
  ValueNode* constructor = LoadRegisterTagged(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();

  size_t input_count = args.register_count() + 1 + Construct::kFixedInputCount;
  Construct* construct =
      CreateNewNode<Construct>(input_count, constructor, new_target, context);
  int arg_index = 0;
  // Add undefined receiver.
  construct->set_arg(arg_index++, GetRootConstant(RootIndex::kUndefinedValue));
  for (int i = 0; i < args.register_count(); i++) {
    construct->set_arg(arg_index++, GetTaggedValue(args[i]));
  }
  SetAccumulator(AddNode(construct));
}

void MaglevGraphBuilder::VisitConstructWithSpread() {
  ValueNode* new_target = GetAccumulatorTagged();
  ValueNode* constructor = LoadRegisterTagged(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();

  int kReceiver = 1;
  size_t input_count =
      args.register_count() + kReceiver + ConstructWithSpread::kFixedInputCount;
  ConstructWithSpread* construct = CreateNewNode<ConstructWithSpread>(
      input_count, constructor, new_target, context);
  int arg_index = 0;
  // Add undefined receiver.
  construct->set_arg(arg_index++, GetRootConstant(RootIndex::kUndefinedValue));
  for (int i = 0; i < args.register_count(); ++i) {
    construct->set_arg(arg_index++, GetTaggedValue(args[i]));
  }
  SetAccumulator(AddNode(construct));
}

void MaglevGraphBuilder::VisitTestEqual() {
  VisitCompareOperation<Operation::kEqual>();
}
void MaglevGraphBuilder::VisitTestEqualStrict() {
  VisitCompareOperation<Operation::kStrictEqual>();
}
void MaglevGraphBuilder::VisitTestLessThan() {
  VisitCompareOperation<Operation::kLessThan>();
}
void MaglevGraphBuilder::VisitTestLessThanOrEqual() {
  VisitCompareOperation<Operation::kLessThanOrEqual>();
}
void MaglevGraphBuilder::VisitTestGreaterThan() {
  VisitCompareOperation<Operation::kGreaterThan>();
}
void MaglevGraphBuilder::VisitTestGreaterThanOrEqual() {
  VisitCompareOperation<Operation::kGreaterThanOrEqual>();
}

void MaglevGraphBuilder::VisitTestInstanceOf() {
  // TestInstanceOf <src> <feedback_slot>
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* callable = GetAccumulatorTagged();
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  // TODO(victorgomes): Check feedback slot and a do static lookup for
  // @@hasInstance.
  USE(feedback_source);

  ValueNode* context = GetContext();
  SetAccumulator(AddNewNode<TestInstanceOf>({context, object, callable}));
}

void MaglevGraphBuilder::VisitTestIn() {
  // TestIn <src> <feedback_slot>
  ValueNode* object = GetAccumulatorTagged();
  ValueNode* name = LoadRegisterTagged(0);
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  // TODO(victorgomes): Create fast path using feedback.
  USE(feedback_source);

  SetAccumulator(
      BuildCallBuiltin<Builtin::kKeyedHasIC>({object, name}, feedback_source));
}

void MaglevGraphBuilder::VisitToName() {
  // ToObject <dst>
  ValueNode* value = GetAccumulatorTagged();
  interpreter::Register destination = iterator_.GetRegisterOperand(0);
  StoreRegister(destination, AddNewNode<ToName>({GetContext(), value}));
}

void MaglevGraphBuilder::BuildToNumberOrToNumeric(Object::Conversion mode) {
  ValueNode* value = GetAccumulatorTagged();
  FeedbackSlot slot = GetSlotOperand(0);
  switch (broker()->GetFeedbackForBinaryOperation(
      compiler::FeedbackSource(feedback(), slot))) {
    case BinaryOperationHint::kSignedSmall:
      BuildCheckSmi(value);
      break;
    case BinaryOperationHint::kSignedSmallInputs:
      UNREACHABLE();
    case BinaryOperationHint::kNumber:
    case BinaryOperationHint::kBigInt:
      AddNewNode<CheckNumber>({value}, mode);
      break;
    default:
      SetAccumulator(
          AddNewNode<ToNumberOrNumeric>({GetContext(), value}, mode));
      break;
  }
}

void MaglevGraphBuilder::VisitToNumber() {
  BuildToNumberOrToNumeric(Object::Conversion::kToNumber);
}
void MaglevGraphBuilder::VisitToNumeric() {
  BuildToNumberOrToNumeric(Object::Conversion::kToNumeric);
}

void MaglevGraphBuilder::VisitToObject() {
  // ToObject <dst>
  ValueNode* value = GetAccumulatorTagged();
  interpreter::Register destination = iterator_.GetRegisterOperand(0);
  StoreRegister(destination, AddNewNode<ToObject>({GetContext(), value}));
}

void MaglevGraphBuilder::VisitToString() {
  // ToString
  ValueNode* value = GetAccumulatorTagged();
  // TODO(victorgomes): Add fast path for constant nodes.
  SetAccumulator(AddNewNode<ToString>({GetContext(), value}));
}

void MaglevGraphBuilder::VisitCreateRegExpLiteral() {
  // CreateRegExpLiteral <pattern_idx> <literal_idx> <flags>
  compiler::StringRef pattern = GetRefOperand<String>(0);
  FeedbackSlot slot = GetSlotOperand(1);
  uint32_t flags = GetFlag16Operand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  // TODO(victorgomes): Inline allocation if feedback has a RegExpLiteral.
  SetAccumulator(
      AddNewNode<CreateRegExpLiteral>({}, pattern, feedback_source, flags));
}

void MaglevGraphBuilder::VisitCreateArrayLiteral() {
  compiler::HeapObjectRef constant_elements = GetRefOperand<HeapObject>(0);
  FeedbackSlot slot_index = GetSlotOperand(1);
  int bytecode_flags = GetFlag8Operand(2);
  int literal_flags =
      interpreter::CreateArrayLiteralFlags::FlagsBits::decode(bytecode_flags);
  if (interpreter::CreateArrayLiteralFlags::FastCloneSupportedBit::decode(
          bytecode_flags)) {
    // TODO(victorgomes): CreateShallowArrayLiteral should not need the
    // boilerplate descriptor. However the current builtin checks that the
    // feedback exists and fallsback to CreateArrayLiteral if it doesn't.
    SetAccumulator(AddNewNode<CreateShallowArrayLiteral>(
        {}, constant_elements, compiler::FeedbackSource{feedback(), slot_index},
        literal_flags));
  } else {
    SetAccumulator(AddNewNode<CreateArrayLiteral>(
        {}, constant_elements, compiler::FeedbackSource{feedback(), slot_index},
        literal_flags));
  }
}

void MaglevGraphBuilder::VisitCreateArrayFromIterable() {
  ValueNode* iterable = GetAccumulatorTagged();
  SetAccumulator(
      BuildCallBuiltin<Builtin::kIterableToListWithSymbolLookup>({iterable}));
}

void MaglevGraphBuilder::VisitCreateEmptyArrayLiteral() {
  // TODO(v8:7700): Consider inlining the allocation.
  FeedbackSlot slot_index = GetSlotOperand(0);
  SetAccumulator(AddNewNode<CreateEmptyArrayLiteral>(
      {}, compiler::FeedbackSource{feedback(), slot_index}));
}

void MaglevGraphBuilder::VisitCreateObjectLiteral() {
  compiler::ObjectBoilerplateDescriptionRef boilerplate_desc =
      GetRefOperand<ObjectBoilerplateDescription>(0);
  FeedbackSlot slot_index = GetSlotOperand(1);
  int bytecode_flags = GetFlag8Operand(2);
  int literal_flags =
      interpreter::CreateObjectLiteralFlags::FlagsBits::decode(bytecode_flags);
  if (interpreter::CreateObjectLiteralFlags::FastCloneSupportedBit::decode(
          bytecode_flags)) {
    // TODO(victorgomes): CreateShallowObjectLiteral should not need the
    // boilerplate descriptor. However the current builtin checks that the
    // feedback exists and fallsback to CreateObjectLiteral if it doesn't.
    SetAccumulator(AddNewNode<CreateShallowObjectLiteral>(
        {}, boilerplate_desc, compiler::FeedbackSource{feedback(), slot_index},
        literal_flags));
  } else {
    SetAccumulator(AddNewNode<CreateObjectLiteral>(
        {}, boilerplate_desc, compiler::FeedbackSource{feedback(), slot_index},
        literal_flags));
  }
}

void MaglevGraphBuilder::VisitCreateEmptyObjectLiteral() {
  compiler::NativeContextRef native_context = broker()->target_native_context();
  compiler::MapRef map =
      native_context.object_function().initial_map(broker()->dependencies());
  DCHECK(!map.is_dictionary_map());
  DCHECK(!map.IsInobjectSlackTrackingInProgress());
  SetAccumulator(AddNewNode<CreateEmptyObjectLiteral>({}, map));
}

void MaglevGraphBuilder::VisitCloneObject() {
  // CloneObject <source_idx> <flags> <feedback_slot>
  ValueNode* source = LoadRegisterTagged(0);
  ValueNode* flags =
      GetSmiConstant(interpreter::CreateObjectLiteralFlags::FlagsBits::decode(
          GetFlag8Operand(1)));
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  SetAccumulator(BuildCallBuiltin<Builtin::kCloneObjectIC>({source, flags},
                                                           feedback_source));
}

void MaglevGraphBuilder::VisitGetTemplateObject() {
  // GetTemplateObject <descriptor_idx> <literal_idx>
  compiler::SharedFunctionInfoRef shared_function_info =
      compilation_unit_->shared_function_info();
  ValueNode* description = GetConstant(GetRefOperand<HeapObject>(0));
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& feedback =
      broker()->GetFeedbackForTemplateObject(feedback_source);
  if (feedback.IsInsufficient()) {
    return SetAccumulator(AddNewNode<GetTemplateObject>(
        {description}, shared_function_info, feedback_source));
  }
  compiler::JSArrayRef template_object = feedback.AsTemplateObject().value();
  SetAccumulator(GetConstant(template_object));
}

void MaglevGraphBuilder::VisitCreateClosure() {
  compiler::SharedFunctionInfoRef shared_function_info =
      GetRefOperand<SharedFunctionInfo>(0);
  compiler::FeedbackCellRef feedback_cell =
      feedback().GetClosureFeedbackCell(iterator_.GetIndexOperand(1));
  uint32_t flags = GetFlag8Operand(2);

  if (interpreter::CreateClosureFlags::FastNewClosureBit::decode(flags)) {
    SetAccumulator(AddNewNode<FastCreateClosure>(
        {GetContext()}, shared_function_info, feedback_cell));
  } else {
    bool pretenured =
        interpreter::CreateClosureFlags::PretenuredBit::decode(flags);
    SetAccumulator(AddNewNode<CreateClosure>(
        {GetContext()}, shared_function_info, feedback_cell, pretenured));
  }
}

void MaglevGraphBuilder::VisitCreateBlockContext() {
  // TODO(v8:7700): Inline allocation when context is small.
  // CreateBlockContext <scope_info_idx>
  ValueNode* scope_info = GetConstant(GetRefOperand<ScopeInfo>(0));
  SetAccumulator(BuildCallRuntime(Runtime::kPushBlockContext, {scope_info}));
}

void MaglevGraphBuilder::VisitCreateCatchContext() {
  // TODO(v8:7700): Inline allocation when context is small.
  // CreateCatchContext <exception> <scope_info_idx>
  ValueNode* exception = LoadRegisterTagged(0);
  ValueNode* scope_info = GetConstant(GetRefOperand<ScopeInfo>(1));
  SetAccumulator(
      BuildCallRuntime(Runtime::kPushCatchContext, {exception, scope_info}));
}

void MaglevGraphBuilder::VisitCreateFunctionContext() {
  compiler::ScopeInfoRef info = GetRefOperand<ScopeInfo>(0);
  uint32_t slot_count = iterator_.GetUnsignedImmediateOperand(1);
  SetAccumulator(AddNewNode<CreateFunctionContext>(
      {GetContext()}, info, slot_count, ScopeType::FUNCTION_SCOPE));
}

void MaglevGraphBuilder::VisitCreateEvalContext() {
  compiler::ScopeInfoRef info = GetRefOperand<ScopeInfo>(0);
  uint32_t slot_count = iterator_.GetUnsignedImmediateOperand(1);
  if (slot_count <= static_cast<uint32_t>(
                        ConstructorBuiltins::MaximumFunctionContextSlots())) {
    SetAccumulator(AddNewNode<CreateFunctionContext>(
        {GetContext()}, info, slot_count, ScopeType::EVAL_SCOPE));
  } else {
    SetAccumulator(
        BuildCallRuntime(Runtime::kNewFunctionContext, {GetConstant(info)}));
  }
}

void MaglevGraphBuilder::VisitCreateWithContext() {
  // TODO(v8:7700): Inline allocation when context is small.
  // CreateWithContext <register> <scope_info_idx>
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* scope_info = GetConstant(GetRefOperand<ScopeInfo>(1));
  SetAccumulator(
      BuildCallRuntime(Runtime::kPushWithContext, {object, scope_info}));
}

void MaglevGraphBuilder::VisitCreateMappedArguments() {
  compiler::SharedFunctionInfoRef shared =
      compilation_unit_->shared_function_info();
  if (shared.object()->has_duplicate_parameters()) {
    SetAccumulator(
        BuildCallRuntime(Runtime::kNewSloppyArguments, {GetClosure()}));
  } else {
    SetAccumulator(
        BuildCallBuiltin<Builtin::kFastNewSloppyArguments>({GetClosure()}));
  }
}

void MaglevGraphBuilder::VisitCreateUnmappedArguments() {
  SetAccumulator(
      BuildCallBuiltin<Builtin::kFastNewStrictArguments>({GetClosure()}));
}

void MaglevGraphBuilder::VisitCreateRestParameter() {
  SetAccumulator(
      BuildCallBuiltin<Builtin::kFastNewRestArguments>({GetClosure()}));
}

void MaglevGraphBuilder::VisitJumpLoop() {
  const uint32_t relative_jump_bytecode_offset =
      iterator_.GetUnsignedImmediateOperand(0);
  const int32_t loop_offset = iterator_.GetImmediateOperand(1);
  const FeedbackSlot feedback_slot = iterator_.GetSlotOperand(2);
  int target = iterator_.GetJumpTargetOffset();

  if (relative_jump_bytecode_offset > 0) {
    AddNewNode<ReduceInterruptBudget>({}, relative_jump_bytecode_offset);
  }
  AddNewNode<JumpLoopPrologue>({}, loop_offset, feedback_slot,
                               BytecodeOffset(iterator_.current_offset()),
                               compilation_unit_);
  BasicBlock* block =
      target == block_offset_
          ? FinishBlock<JumpLoop>(next_offset(), {}, &jump_targets_[target])
          : FinishBlock<JumpLoop>(next_offset(), {},
                                  jump_targets_[target].block_ptr());

  merge_states_[target]->MergeLoop(*compilation_unit_,
                                   current_interpreter_frame_, block, target);
  block->set_predecessor_id(merge_states_[target]->predecessor_count() - 1);
}
void MaglevGraphBuilder::VisitJump() {
  const uint32_t relative_jump_bytecode_offset =
      iterator_.GetRelativeJumpTargetOffset();
  if (relative_jump_bytecode_offset > 0) {
    AddNewNode<IncreaseInterruptBudget>({}, relative_jump_bytecode_offset);
  }
  BasicBlock* block = FinishBlock<Jump>(
      next_offset(), {}, &jump_targets_[iterator_.GetJumpTargetOffset()]);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
  DCHECK_LT(next_offset(), bytecode().length());
}
void MaglevGraphBuilder::VisitJumpConstant() { VisitJump(); }
void MaglevGraphBuilder::VisitJumpIfNullConstant() { VisitJumpIfNull(); }
void MaglevGraphBuilder::VisitJumpIfNotNullConstant() { VisitJumpIfNotNull(); }
void MaglevGraphBuilder::VisitJumpIfUndefinedConstant() {
  VisitJumpIfUndefined();
}
void MaglevGraphBuilder::VisitJumpIfNotUndefinedConstant() {
  VisitJumpIfNotUndefined();
}
void MaglevGraphBuilder::VisitJumpIfUndefinedOrNullConstant() {
  VisitJumpIfUndefinedOrNull();
}
void MaglevGraphBuilder::VisitJumpIfTrueConstant() { VisitJumpIfTrue(); }
void MaglevGraphBuilder::VisitJumpIfFalseConstant() { VisitJumpIfFalse(); }
void MaglevGraphBuilder::VisitJumpIfJSReceiverConstant() {
  VisitJumpIfJSReceiver();
}
void MaglevGraphBuilder::VisitJumpIfToBooleanTrueConstant() {
  VisitJumpIfToBooleanTrue();
}
void MaglevGraphBuilder::VisitJumpIfToBooleanFalseConstant() {
  VisitJumpIfToBooleanFalse();
}

void MaglevGraphBuilder::MergeIntoFrameState(BasicBlock* predecessor,
                                             int target) {
  if (merge_states_[target] == nullptr) {
    DCHECK(!bytecode_analysis().IsLoopHeader(target));
    const compiler::BytecodeLivenessState* liveness = GetInLivenessFor(target);
    // If there's no target frame state, allocate a new one.
    merge_states_[target] = MergePointInterpreterFrameState::New(
        *compilation_unit_, current_interpreter_frame_, target,
        NumPredecessors(target), predecessor, liveness);
  } else {
    // If there already is a frame state, merge.
    merge_states_[target]->Merge(*compilation_unit_, current_interpreter_frame_,
                                 predecessor, target);
  }
}

void MaglevGraphBuilder::MergeDeadIntoFrameState(int target) {
  // If there is no merge state yet, don't create one, but just reduce the
  // number of possible predecessors to zero.
  predecessors_[target]--;
  if (merge_states_[target]) {
    // If there already is a frame state, merge.
    merge_states_[target]->MergeDead(*compilation_unit_, target);
    // If this merge is the last one which kills a loop merge, remove that
    // merge state.
    if (merge_states_[target]->is_unreachable_loop()) {
      if (FLAG_trace_maglev_graph_building) {
        std::cout << "! Killing loop merge state at @" << target << std::endl;
      }
      merge_states_[target] = nullptr;
    }
  }
}

void MaglevGraphBuilder::MergeDeadLoopIntoFrameState(int target) {
  // If there is no merge state yet, don't create one, but just reduce the
  // number of possible predecessors to zero.
  predecessors_[target]--;
  if (merge_states_[target]) {
    // If there already is a frame state, merge.
    merge_states_[target]->MergeDeadLoop(*compilation_unit_, target);
  }
}

void MaglevGraphBuilder::MergeIntoInlinedReturnFrameState(
    BasicBlock* predecessor) {
  int target = inline_exit_offset();
  if (merge_states_[target] == nullptr) {
    // All returns should have the same liveness, which is that only the
    // accumulator is live.
    const compiler::BytecodeLivenessState* liveness = GetInLiveness();
    DCHECK(liveness->AccumulatorIsLive());
    DCHECK_EQ(liveness->live_value_count(), 1);

    // If there's no target frame state, allocate a new one.
    merge_states_[target] = MergePointInterpreterFrameState::New(
        *compilation_unit_, current_interpreter_frame_, target,
        NumPredecessors(target), predecessor, liveness);
  } else {
    // Again, all returns should have the same liveness, so double check this.
    DCHECK(GetInLiveness()->Equals(
        *merge_states_[target]->frame_state().liveness()));
    merge_states_[target]->Merge(*compilation_unit_, current_interpreter_frame_,
                                 predecessor, target);
  }
}

void MaglevGraphBuilder::BuildBranchIfRootConstant(ValueNode* node,
                                                   int true_target,
                                                   int false_target,
                                                   RootIndex root_index) {
  BasicBlock* block = FinishBlock<BranchIfRootConstant>(
      next_offset(), {node}, &jump_targets_[true_target],
      &jump_targets_[false_target], root_index);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
}
void MaglevGraphBuilder::BuildBranchIfTrue(ValueNode* node, int true_target,
                                           int false_target) {
  BuildBranchIfRootConstant(node, true_target, false_target,
                            RootIndex::kTrueValue);
}
void MaglevGraphBuilder::BuildBranchIfNull(ValueNode* node, int true_target,
                                           int false_target) {
  BuildBranchIfRootConstant(node, true_target, false_target,
                            RootIndex::kNullValue);
}
void MaglevGraphBuilder::BuildBranchIfUndefined(ValueNode* node,
                                                int true_target,
                                                int false_target) {
  BuildBranchIfRootConstant(node, true_target, false_target,
                            RootIndex::kUndefinedValue);
}
void MaglevGraphBuilder::BuildBranchIfToBooleanTrue(ValueNode* node,
                                                    int true_target,
                                                    int false_target) {
  BasicBlock* block = FinishBlock<BranchIfToBooleanTrue>(
      next_offset(), {node}, &jump_targets_[true_target],
      &jump_targets_[false_target]);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
}
void MaglevGraphBuilder::VisitJumpIfToBooleanTrue() {
  BuildBranchIfToBooleanTrue(GetAccumulatorTagged(),
                             iterator_.GetJumpTargetOffset(), next_offset());
}
void MaglevGraphBuilder::VisitJumpIfToBooleanFalse() {
  BuildBranchIfToBooleanTrue(GetAccumulatorTagged(), next_offset(),
                             iterator_.GetJumpTargetOffset());
}
void MaglevGraphBuilder::VisitJumpIfTrue() {
  BuildBranchIfTrue(GetAccumulatorTagged(), iterator_.GetJumpTargetOffset(),
                    next_offset());
}
void MaglevGraphBuilder::VisitJumpIfFalse() {
  BuildBranchIfTrue(GetAccumulatorTagged(), next_offset(),
                    iterator_.GetJumpTargetOffset());
}
void MaglevGraphBuilder::VisitJumpIfNull() {
  BuildBranchIfNull(GetAccumulatorTagged(), iterator_.GetJumpTargetOffset(),
                    next_offset());
}
void MaglevGraphBuilder::VisitJumpIfNotNull() {
  BuildBranchIfNull(GetAccumulatorTagged(), next_offset(),
                    iterator_.GetJumpTargetOffset());
}
void MaglevGraphBuilder::VisitJumpIfUndefined() {
  BuildBranchIfUndefined(GetAccumulatorTagged(),
                         iterator_.GetJumpTargetOffset(), next_offset());
}
void MaglevGraphBuilder::VisitJumpIfNotUndefined() {
  BuildBranchIfUndefined(GetAccumulatorTagged(), next_offset(),
                         iterator_.GetJumpTargetOffset());
}
void MaglevGraphBuilder::VisitJumpIfUndefinedOrNull() {
  BasicBlock* block = FinishBlock<BranchIfUndefinedOrNull>(
      next_offset(), {GetAccumulatorTagged()},
      &jump_targets_[iterator_.GetJumpTargetOffset()],
      &jump_targets_[next_offset()]);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
}
void MaglevGraphBuilder::VisitJumpIfJSReceiver() {
  BasicBlock* block = FinishBlock<BranchIfJSReceiver>(
      next_offset(), {GetAccumulatorTagged()},
      &jump_targets_[iterator_.GetJumpTargetOffset()],
      &jump_targets_[next_offset()]);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
}

void MaglevGraphBuilder::VisitSwitchOnSmiNoFeedback() {
  // SwitchOnSmiNoFeedback <table_start> <table_length> <case_value_base>
  interpreter::JumpTableTargetOffsets offsets =
      iterator_.GetJumpTableTargetOffsets();

  if (offsets.size() == 0) return;

  int case_value_base = (*offsets.begin()).case_value;
  BasicBlockRef* targets = zone()->NewArray<BasicBlockRef>(offsets.size());
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    BasicBlockRef* ref = &targets[offset.case_value - case_value_base];
    new (ref) BasicBlockRef(&jump_targets_[offset.target_offset]);
  }

  ValueNode* case_value = GetAccumulatorInt32();
  BasicBlock* block =
      FinishBlock<Switch>(next_offset(), {case_value}, case_value_base, targets,
                          offsets.size(), &jump_targets_[next_offset()]);
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    MergeIntoFrameState(block, offset.target_offset);
  }
}

void MaglevGraphBuilder::VisitForInEnumerate() {
  // ForInEnumerate <receiver>
  ValueNode* receiver = LoadRegisterTagged(0);
  SetAccumulator(BuildCallBuiltin<Builtin::kForInEnumerate>({receiver}));
}

void MaglevGraphBuilder::VisitForInPrepare() {
  // ForInPrepare <cache_info_triple>
  ValueNode* enumerator = GetAccumulatorTagged();
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  // TODO(v8:7700): Use feedback and create fast path.
  ValueNode* context = GetContext();
  ForInPrepare* result =
      AddNewNode<ForInPrepare>({context, enumerator}, feedback_source);
  // No need to set the accumulator.
  DCHECK(!GetOutLiveness()->AccumulatorIsLive());
  // The result is output in registers |cache_info_triple| to
  // |cache_info_triple + 2|, with the registers holding cache_type,
  // cache_array, and cache_length respectively.
  interpreter::Register first = iterator_.GetRegisterOperand(0);
  auto array_and_length =
      std::make_pair(interpreter::Register{first.index() + 1},
                     interpreter::Register{first.index() + 2});
  StoreRegisterPair(array_and_length, result);
}

void MaglevGraphBuilder::VisitForInContinue() {
  // ForInContinue <index> <cache_length>
  ValueNode* index = LoadRegisterTagged(0);
  ValueNode* cache_length = LoadRegisterTagged(1);
  SetAccumulator(AddNewNode<TaggedNotEqual>({index, cache_length}));
}

void MaglevGraphBuilder::VisitForInNext() {
  // ForInNext <receiver> <index> <cache_info_pair>
  ValueNode* receiver = LoadRegisterTagged(0);
  ValueNode* index = LoadRegisterTagged(1);
  interpreter::Register cache_type_reg, cache_array_reg;
  std::tie(cache_type_reg, cache_array_reg) =
      iterator_.GetRegisterPairOperand(2);
  ValueNode* cache_type = GetTaggedValue(cache_type_reg);
  ValueNode* cache_array = GetTaggedValue(cache_array_reg);
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  ValueNode* context = GetContext();
  SetAccumulator(AddNewNode<ForInNext>(
      {context, receiver, cache_array, cache_type, index}, feedback_source));
}

void MaglevGraphBuilder::VisitForInStep() {
  // TODO(victorgomes): We should be able to assert that Register(0)
  // contains an Smi.
  ValueNode* index = LoadRegisterInt32(0);
  ValueNode* one = GetInt32Constant(1);
  SetAccumulator(AddNewInt32BinaryOperationNode<Operation::kAdd>({index, one}));
}

void MaglevGraphBuilder::VisitSetPendingMessage() {
  ValueNode* message = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<SetPendingMessage>({message}));
}

void MaglevGraphBuilder::VisitThrow() {
  ValueNode* exception = GetAccumulatorTagged();
  BuildCallRuntime(Runtime::kThrow, {exception});
  BuildAbort(AbortReason::kUnexpectedReturnFromThrow);
}
void MaglevGraphBuilder::VisitReThrow() {
  ValueNode* exception = GetAccumulatorTagged();
  BuildCallRuntime(Runtime::kReThrow, {exception});
  BuildAbort(AbortReason::kUnexpectedReturnFromThrow);
}

void MaglevGraphBuilder::VisitReturn() {
  // See also: InterpreterAssembler::UpdateInterruptBudgetOnReturn.
  const uint32_t relative_jump_bytecode_offset = iterator_.current_offset();
  if (relative_jump_bytecode_offset > 0) {
    AddNewNode<ReduceInterruptBudget>({}, relative_jump_bytecode_offset);
  }

  if (!is_inline()) {
    FinishBlock<Return>(next_offset(), {GetAccumulatorTagged()});
    return;
  }

  // All inlined function returns instead jump to one past the end of the
  // bytecode, where we'll later create a final basic block which resumes
  // execution of the caller.
  // TODO(leszeks): Consider shortcutting this Jump for cases where there is
  // only one return and no need to merge return states.
  BasicBlock* block = FinishBlock<Jump>(next_offset(), {},
                                        &jump_targets_[inline_exit_offset()]);
  MergeIntoInlinedReturnFrameState(block);
}

void MaglevGraphBuilder::VisitThrowReferenceErrorIfHole() {
  // ThrowReferenceErrorIfHole <variable_name>
  compiler::NameRef name = GetRefOperand<Name>(0);
  ValueNode* value = GetAccumulatorTagged();
  // Avoid the check if we know it is not the hole.
  if (IsConstantNode(value->opcode())) {
    if (IsConstantNodeTheHole(value)) {
      ValueNode* constant = GetConstant(name);
      BuildCallRuntime(Runtime::kThrowAccessedUninitializedVariable,
                       {constant});
      BuildAbort(AbortReason::kUnexpectedReturnFromThrow);
    }
    return;
  }
  AddNewNode<ThrowReferenceErrorIfHole>({value}, name);
}
void MaglevGraphBuilder::VisitThrowSuperNotCalledIfHole() {
  // ThrowSuperNotCalledIfHole
  ValueNode* value = GetAccumulatorTagged();
  // Avoid the check if we know it is not the hole.
  if (IsConstantNode(value->opcode())) {
    if (IsConstantNodeTheHole(value)) {
      BuildCallRuntime(Runtime::kThrowSuperNotCalled, {});
      BuildAbort(AbortReason::kUnexpectedReturnFromThrow);
    }
    return;
  }
  AddNewNode<ThrowSuperNotCalledIfHole>({value});
}
void MaglevGraphBuilder::VisitThrowSuperAlreadyCalledIfNotHole() {
  // ThrowSuperAlreadyCalledIfNotHole
  ValueNode* value = GetAccumulatorTagged();
  // Avoid the check if we know it is the hole.
  if (IsConstantNode(value->opcode())) {
    if (!IsConstantNodeTheHole(value)) {
      BuildCallRuntime(Runtime::kThrowSuperAlreadyCalledError, {});
      BuildAbort(AbortReason::kUnexpectedReturnFromThrow);
    }
    return;
  }
  AddNewNode<ThrowSuperAlreadyCalledIfNotHole>({value});
}
void MaglevGraphBuilder::VisitThrowIfNotSuperConstructor() {
  // ThrowIfNotSuperConstructor <constructor>
  ValueNode* constructor = LoadRegisterTagged(0);
  ValueNode* function = GetClosure();
  AddNewNode<ThrowIfNotSuperConstructor>({constructor, function});
}

void MaglevGraphBuilder::VisitSwitchOnGeneratorState() {
  // SwitchOnGeneratorState <generator> <table_start> <table_length>
  // It should be the first bytecode in the bytecode array.
  DCHECK_EQ(block_offset_, 0);
  int generator_prologue_block_offset = block_offset_ + 1;
  DCHECK_LT(generator_prologue_block_offset, next_offset());

  // We create an initial block that checks if the generator is undefined.
  ValueNode* maybe_generator = LoadRegisterTagged(0);
  BasicBlock* block_is_generator_undefined = CreateBlock<BranchIfRootConstant>(
      {maybe_generator}, &jump_targets_[next_offset()],
      &jump_targets_[generator_prologue_block_offset],
      RootIndex::kUndefinedValue);
  MergeIntoFrameState(block_is_generator_undefined, next_offset());
  ResolveJumpsToBlockAtOffset(block_is_generator_undefined, block_offset_);

  // We create the generator prologue block.
  StartNewBlock(generator_prologue_block_offset);
  DCHECK_EQ(generator_prologue_block_offset, block_offset_);

  // Generator prologue.
  ValueNode* generator = maybe_generator;
  ValueNode* state = AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kContinuationOffset);
  ValueNode* new_state = GetSmiConstant(JSGeneratorObject::kGeneratorExecuting);
  AddNewNode<StoreTaggedFieldNoWriteBarrier>(
      {generator, new_state}, JSGeneratorObject::kContinuationOffset);
  ValueNode* context = AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kContextOffset);
  SetContext(context);

  // Guarantee that we have something in the accumulator.
  MoveNodeBetweenRegisters(iterator_.GetRegisterOperand(0),
                           interpreter::Register::virtual_accumulator());

  // Switch on generator state.
  interpreter::JumpTableTargetOffsets offsets =
      iterator_.GetJumpTableTargetOffsets();
  DCHECK_NE(offsets.size(), 0);
  int case_value_base = (*offsets.begin()).case_value;
  BasicBlockRef* targets = zone()->NewArray<BasicBlockRef>(offsets.size());
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    BasicBlockRef* ref = &targets[offset.case_value - case_value_base];
    new (ref) BasicBlockRef(&jump_targets_[offset.target_offset]);
  }
  ValueNode* case_value = AddNewNode<CheckedSmiUntag>({state});
  BasicBlock* generator_prologue_block = CreateBlock<Switch>(
      {case_value}, case_value_base, targets, offsets.size());
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    MergeIntoFrameState(generator_prologue_block, offset.target_offset);
  }
  ResolveJumpsToBlockAtOffset(generator_prologue_block, block_offset_);
}

void MaglevGraphBuilder::VisitSuspendGenerator() {
  // SuspendGenerator <generator> <first input register> <register count>
  // <suspend_id>
  ValueNode* generator = LoadRegisterTagged(0);
  ValueNode* context = GetContext();
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  uint32_t suspend_id = iterator_.GetUnsignedImmediateOperand(3);

  int input_count = parameter_count_without_receiver() + args.register_count() +
                    GeneratorStore::kFixedInputCount;
  GeneratorStore* node = CreateNewNode<GeneratorStore>(
      input_count, context, generator, suspend_id, iterator_.current_offset());
  int arg_index = 0;
  for (int i = 1 /* skip receiver */; i < parameter_count(); ++i) {
    node->set_parameters_and_registers(arg_index++, GetTaggedArgument(i));
  }
  const compiler::BytecodeLivenessState* liveness = GetOutLiveness();
  for (int i = 0; i < args.register_count(); ++i) {
    ValueNode* value = liveness->RegisterIsLive(args[i].index())
                           ? GetTaggedValue(args[i])
                           : GetRootConstant(RootIndex::kOptimizedOut);
    node->set_parameters_and_registers(arg_index++, value);
  }
  AddNode(node);

  const uint32_t relative_jump_bytecode_offset = iterator_.current_offset();
  if (relative_jump_bytecode_offset > 0) {
    AddNewNode<ReduceInterruptBudget>({}, relative_jump_bytecode_offset);
  }
  FinishBlock<Return>(next_offset(), {GetAccumulatorTagged()});
}

void MaglevGraphBuilder::VisitResumeGenerator() {
  // ResumeGenerator <generator> <first output register> <register count>
  ValueNode* generator = LoadRegisterTagged(0);
  ValueNode* array = AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kParametersAndRegistersOffset);
  interpreter::RegisterList registers = iterator_.GetRegisterListOperand(1);

  if (FLAG_maglev_assert) {
    // Check if register count is invalid, that is, larger than the
    // register file length.
    ValueNode* array_length_smi =
        AddNewNode<LoadTaggedField>({array}, FixedArrayBase::kLengthOffset);
    ValueNode* array_length = AddNewNode<CheckedSmiUntag>({array_length_smi});
    ValueNode* register_size = GetInt32Constant(
        parameter_count_without_receiver() + registers.register_count());
    AddNewNode<AssertInt32>(
        {register_size, array_length}, AssertCondition::kLessOrEqual,
        AbortReason::kInvalidParametersAndRegistersInGenerator);
  }

  const compiler::BytecodeLivenessState* liveness =
      GetOutLivenessFor(next_offset());
  for (int i = 0; i < registers.register_count(); ++i) {
    if (liveness->RegisterIsLive(registers[i].index())) {
      int array_index = parameter_count_without_receiver() + i;
      StoreRegister(registers[i],
                    AddNewNode<GeneratorRestoreRegister>({array}, array_index));
    }
  }
  SetAccumulator(AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kInputOrDebugPosOffset));
}

void MaglevGraphBuilder::VisitGetIterator() {
  // GetIterator <object>
  ValueNode* receiver = LoadRegisterTagged(0);
  ValueNode* context = GetContext();
  int load_slot = iterator_.GetIndexOperand(1);
  int call_slot = iterator_.GetIndexOperand(2);
  SetAccumulator(AddNewNode<GetIterator>({context, receiver}, load_slot,
                                         call_slot, feedback()));
}

void MaglevGraphBuilder::VisitDebugger() {
  BuildCallRuntime(Runtime::kHandleDebuggerStatement, {});
}

void MaglevGraphBuilder::VisitIncBlockCounter() {
  ValueNode* closure = GetClosure();
  ValueNode* coverage_array_slot = GetSmiConstant(iterator_.GetIndexOperand(0));
  BuildCallBuiltin<Builtin::kIncBlockCounter>({closure, coverage_array_slot});
}

void MaglevGraphBuilder::VisitAbort() {
  AbortReason reason = static_cast<AbortReason>(GetFlag8Operand(0));
  BuildAbort(reason);
}

void MaglevGraphBuilder::VisitWide() { UNREACHABLE(); }
void MaglevGraphBuilder::VisitExtraWide() { UNREACHABLE(); }
#define DEBUG_BREAK(Name, ...) \
  void MaglevGraphBuilder::Visit##Name() { UNREACHABLE(); }
DEBUG_BREAK_BYTECODE_LIST(DEBUG_BREAK)
#undef DEBUG_BREAK
void MaglevGraphBuilder::VisitIllegal() { UNREACHABLE(); }

}  // namespace maglev
}  // namespace internal
}  // namespace v8
