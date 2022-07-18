// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-builder.h"

#include "src/base/optional.h"
#include "src/base/v8-fallthrough.h"
#include "src/builtins/builtins-constructor.h"
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
      current_interpreter_frame_(*compilation_unit_) {
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

  for (auto& offset_and_info : bytecode_analysis().GetLoopInfos()) {
    int offset = offset_and_info.first;
    const compiler::LoopInfo& loop_info = offset_and_info.second;

    const compiler::BytecodeLivenessState* liveness = GetInLivenessFor(offset);

    merge_states_[offset] = zone()->New<MergePointInterpreterFrameState>(
        *compilation_unit_, offset, NumPredecessors(offset), liveness,
        &loop_info);
  }
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

// TODO(v8:7700): Clean up after all bytecodes are supported.
#define MAGLEV_UNIMPLEMENTED(BytecodeName)                              \
  do {                                                                  \
    std::cerr << "Maglev: Can't compile "                               \
              << Brief(*compilation_unit_->function().object())         \
              << ", bytecode " #BytecodeName " is not supported\n";     \
    found_unsupported_bytecode_ = true;                                 \
    this_field_will_be_unused_once_all_bytecodes_are_supported_ = true; \
  } while (false)

#define MAGLEV_UNIMPLEMENTED_BYTECODE(Name) \
  void MaglevGraphBuilder::Visit##Name() { MAGLEV_UNIMPLEMENTED(Name); }

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
// (Operation name,
//  Int32 operation node,
//  Unit of int32 operation (e.g, 0 for add/sub and 1 for mul/div))
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
  V(ShiftRightLogical, Int32ShiftRightLogical, 0)

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
static int Int32Unit() {
  switch (kOperation) {
#define CASE(op, OpNode, unit) \
  case Operation::k##op:       \
    return unit;
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
#define CASE(op, OpNode, unit) \
  case Operation::k##op:       \
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
  if (constant == Int32Unit<kOperation>()) {
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
    case CompareOperationHint::kInternalizedString:
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

MAGLEV_UNIMPLEMENTED_BYTECODE(TestTypeOf)

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
  SetAccumulator(AddNewNode<LoadGlobal>({context}, name, feedback_source));
}
MAGLEV_UNIMPLEMENTED_BYTECODE(LdaGlobalInsideTypeof)
MAGLEV_UNIMPLEMENTED_BYTECODE(StaGlobal)
MAGLEV_UNIMPLEMENTED_BYTECODE(LdaLookupSlot)
MAGLEV_UNIMPLEMENTED_BYTECODE(LdaLookupContextSlot)
MAGLEV_UNIMPLEMENTED_BYTECODE(LdaLookupGlobalSlot)
MAGLEV_UNIMPLEMENTED_BYTECODE(LdaLookupSlotInsideTypeof)
MAGLEV_UNIMPLEMENTED_BYTECODE(LdaLookupContextSlotInsideTypeof)
MAGLEV_UNIMPLEMENTED_BYTECODE(LdaLookupGlobalSlotInsideTypeof)
MAGLEV_UNIMPLEMENTED_BYTECODE(StaLookupSlot)

void MaglevGraphBuilder::BuildMapCheck(ValueNode* object,
                                       const compiler::MapRef& map) {
  AddNewNode<CheckHeapObject>({object});
  if (map.is_migration_target()) {
    AddNewNode<CheckMapsWithMigration>({object}, map);
  } else {
    AddNewNode<CheckMaps>({object}, map);
  }
}

bool MaglevGraphBuilder::TryBuildMonomorphicLoad(ValueNode* object,
                                                 const compiler::MapRef& map,
                                                 MaybeObjectHandle handler) {
  if (handler.is_null()) return false;

  if (handler->IsSmi()) {
    return TryBuildMonomorphicLoadFromSmiHandler(object, map,
                                                 handler->ToSmi().value());
  } else if (handler->GetHeapObject().IsCodeT()) {
    // TODO(leszeks): Call the code object directly.
    return false;
  } else {
    return TryBuildMonomorphicLoadFromLoadHandler(
        object, map, LoadHandler::cast(handler->GetHeapObject()));
  }
}

bool MaglevGraphBuilder::TryBuildMonomorphicLoadFromSmiHandler(
    ValueNode* object, const compiler::MapRef& map, int32_t handler) {
  // Smi handler, emit a map check and LoadField.
  LoadHandler::Kind kind = LoadHandler::KindBits::decode(handler);
  if (kind != LoadHandler::Kind::kField) return false;
  if (LoadHandler::IsWasmStructBits::decode(handler)) return false;

  BuildMapCheck(object, map);

  ValueNode* load_source;
  if (LoadHandler::IsInobjectBits::decode(handler)) {
    load_source = object;
  } else {
    // The field is in the property array, first load it from there.
    load_source = AddNewNode<LoadTaggedField>(
        {object}, JSReceiver::kPropertiesOrHashOffset);
  }
  int field_index = LoadHandler::FieldIndexBits::decode(handler);
  if (LoadHandler::IsDoubleBits::decode(handler)) {
    FieldIndex field = FieldIndex::ForSmiLoadHandler(*map.object(), handler);
    DescriptorArray descriptors = *map.instance_descriptors().object();
    InternalIndex index =
        descriptors.Search(field.property_index(), *map.object());
    DCHECK(index.is_found());
    DCHECK(descriptors.GetDetails(index).representation().IsDouble());
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
    ValueNode* object, const compiler::MapRef& map, LoadHandler handler) {
  Object maybe_smi_handler = handler.smi_handler(local_isolate_);
  if (!maybe_smi_handler.IsSmi()) return false;
  int smi_handler = Smi::cast(maybe_smi_handler).value();
  LoadHandler::Kind kind = LoadHandler::KindBits::decode(smi_handler);
  bool do_access_check_on_lookup_start_object =
      LoadHandler::DoAccessCheckOnLookupStartObjectBits::decode(smi_handler);
  bool lookup_on_lookup_start_object =
      LoadHandler::LookupOnLookupStartObjectBits::decode(smi_handler);
  if (lookup_on_lookup_start_object) return false;
  if (kind != LoadHandler::Kind::kConstantFromPrototype) return false;

  if (map.IsStringMap()) {
    // Check for string maps before checking if we need to do an access check.
    // Primitive strings always get the prototype from the native context
    // they're operated on, so they don't need the access check.
    AddNewNode<CheckHeapObject>({object});
    AddNewNode<CheckString>({object});
  } else if (do_access_check_on_lookup_start_object) {
    return false;
  } else {
    BuildMapCheck(object, map);
  }

  Object validity_cell = handler.validity_cell(local_isolate_);
  if (validity_cell.IsCell(local_isolate_)) {
    broker()->dependencies()->DependOnStablePrototypeChain(
        map, kStartAtPrototype, base::nullopt);
  } else {
    DCHECK_EQ(Smi::ToInt(validity_cell), Map::kPrototypeChainValid);
  }
  MaybeObject value = handler.data1(local_isolate_);
  if (value.IsSmi()) {
    SetAccumulator(GetSmiConstant(value.ToSmi().value()));
  } else {
    SetAccumulator(GetConstant(MakeRefAssumeMemoryFence(
        broker(), broker()->CanonicalPersistentHandle(value.GetHeapObject()))));
  }
  return true;
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

      if (TryBuildMonomorphicLoad(object, map, handler)) return;
    } break;

    default:
      break;
  }

  // Create a generic load in the fallthrough.
  ValueNode* context = GetContext();
  SetAccumulator(
      AddNewNode<LoadNamedGeneric>({context, object}, name, feedback_source));
}

MAGLEV_UNIMPLEMENTED_BYTECODE(GetNamedPropertyFromSuper)

void MaglevGraphBuilder::VisitGetKeyedProperty() {
  // GetKeyedProperty <object> <slot>
  ValueNode* object = LoadRegisterTagged(0);
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

    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  SetAccumulator(
      AddNewNode<GetKeyedGeneric>({context, object, key}, feedback_source));
}

MAGLEV_UNIMPLEMENTED_BYTECODE(LdaModuleVariable)
MAGLEV_UNIMPLEMENTED_BYTECODE(StaModuleVariable)

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
    AddNewNode<CheckSmi>({value});
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
      AddNewNode<CheckHeapObject>({value});
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

MAGLEV_UNIMPLEMENTED_BYTECODE(DefineKeyedOwnPropertyInLiteral)
MAGLEV_UNIMPLEMENTED_BYTECODE(CollectTypeProfile)

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

MAGLEV_UNIMPLEMENTED_BYTECODE(ToBooleanLogicalNot)

void MaglevGraphBuilder::VisitLogicalNot() {
  // Invariant: accumulator must already be a boolean value.
  ValueNode* value = GetAccumulatorTagged();
  if (RootConstant* constant = value->TryCast<RootConstant>()) {
    if (constant->index() == RootIndex::kTrueValue) {
      SetAccumulator(GetRootConstant(RootIndex::kFalseValue));
    } else {
      DCHECK_EQ(constant->index(), RootIndex::kFalseValue);
      SetAccumulator(GetRootConstant(RootIndex::kTrueValue));
    }
  }
  SetAccumulator(AddNewNode<LogicalNot>({value}));
}

MAGLEV_UNIMPLEMENTED_BYTECODE(TypeOf)
MAGLEV_UNIMPLEMENTED_BYTECODE(DeletePropertyStrict)
MAGLEV_UNIMPLEMENTED_BYTECODE(DeletePropertySloppy)
MAGLEV_UNIMPLEMENTED_BYTECODE(GetSuperConstructor)

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
  current_block_ =
      zone()->New<BasicBlock>(zone()->New<MergePointInterpreterFrameState>(
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

MAGLEV_UNIMPLEMENTED_BYTECODE(CallWithSpread)
MAGLEV_UNIMPLEMENTED_BYTECODE(CallRuntime)
MAGLEV_UNIMPLEMENTED_BYTECODE(CallRuntimeForPair)
MAGLEV_UNIMPLEMENTED_BYTECODE(CallJSRuntime)
MAGLEV_UNIMPLEMENTED_BYTECODE(InvokeIntrinsic)

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

MAGLEV_UNIMPLEMENTED_BYTECODE(ConstructWithSpread)

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

MAGLEV_UNIMPLEMENTED_BYTECODE(TestIn)
MAGLEV_UNIMPLEMENTED_BYTECODE(ToName)
MAGLEV_UNIMPLEMENTED_BYTECODE(ToNumber)
MAGLEV_UNIMPLEMENTED_BYTECODE(ToNumeric)
MAGLEV_UNIMPLEMENTED_BYTECODE(ToObject)
MAGLEV_UNIMPLEMENTED_BYTECODE(ToString)
MAGLEV_UNIMPLEMENTED_BYTECODE(CreateRegExpLiteral)

void MaglevGraphBuilder::VisitCreateArrayLiteral() {
  compiler::HeapObjectRef constant_elements = GetRefOperand<HeapObject>(0);
  FeedbackSlot slot_index = GetSlotOperand(1);
  int bytecode_flags = GetFlagOperand(2);
  int literal_flags =
      interpreter::CreateArrayLiteralFlags::FlagsBits::decode(bytecode_flags);
  ValueNode* result;
  if (interpreter::CreateArrayLiteralFlags::FastCloneSupportedBit::decode(
          bytecode_flags)) {
    // TODO(victorgomes): CreateShallowArrayLiteral should not need the
    // boilerplate descriptor. However the current builtin checks that the
    // feedback exists and fallsback to CreateArrayLiteral if it doesn't.
    result = AddNewNode<CreateShallowArrayLiteral>(
        {}, constant_elements, compiler::FeedbackSource{feedback(), slot_index},
        literal_flags);
  } else {
    result = AddNewNode<CreateArrayLiteral>(
        {}, constant_elements, compiler::FeedbackSource{feedback(), slot_index},
        literal_flags);
  }
  SetAccumulator(result);
}

MAGLEV_UNIMPLEMENTED_BYTECODE(CreateArrayFromIterable)

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
  int bytecode_flags = GetFlagOperand(2);
  int literal_flags =
      interpreter::CreateObjectLiteralFlags::FlagsBits::decode(bytecode_flags);
  ValueNode* result;
  if (interpreter::CreateObjectLiteralFlags::FastCloneSupportedBit::decode(
          bytecode_flags)) {
    // TODO(victorgomes): CreateShallowObjectLiteral should not need the
    // boilerplate descriptor. However the current builtin checks that the
    // feedback exists and fallsback to CreateObjectLiteral if it doesn't.
    result = AddNewNode<CreateShallowObjectLiteral>(
        {}, boilerplate_desc, compiler::FeedbackSource{feedback(), slot_index},
        literal_flags);
  } else {
    result = AddNewNode<CreateObjectLiteral>(
        {}, boilerplate_desc, compiler::FeedbackSource{feedback(), slot_index},
        literal_flags);
  }
  SetAccumulator(result);
}

MAGLEV_UNIMPLEMENTED_BYTECODE(CreateEmptyObjectLiteral)
MAGLEV_UNIMPLEMENTED_BYTECODE(CloneObject)
MAGLEV_UNIMPLEMENTED_BYTECODE(GetTemplateObject)

void MaglevGraphBuilder::VisitCreateClosure() {
  compiler::SharedFunctionInfoRef shared_function_info =
      GetRefOperand<SharedFunctionInfo>(0);
  compiler::FeedbackCellRef feedback_cell =
      feedback().GetClosureFeedbackCell(iterator_.GetIndexOperand(1));
  uint32_t flags = GetFlagOperand(2);

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

MAGLEV_UNIMPLEMENTED_BYTECODE(CreateBlockContext)
MAGLEV_UNIMPLEMENTED_BYTECODE(CreateCatchContext)

void MaglevGraphBuilder::VisitCreateFunctionContext() {
  compiler::ScopeInfoRef info = GetRefOperand<ScopeInfo>(0);
  uint32_t slot_count = iterator_.GetUnsignedImmediateOperand(1);
  SetAccumulator(
      AddNewNode<CreateFunctionContext>({GetContext()}, info, slot_count));
}

MAGLEV_UNIMPLEMENTED_BYTECODE(CreateEvalContext)
MAGLEV_UNIMPLEMENTED_BYTECODE(CreateWithContext)
MAGLEV_UNIMPLEMENTED_BYTECODE(CreateMappedArguments)
MAGLEV_UNIMPLEMENTED_BYTECODE(CreateUnmappedArguments)
MAGLEV_UNIMPLEMENTED_BYTECODE(CreateRestParameter)

void MaglevGraphBuilder::VisitJumpLoop() {
  const uint32_t relative_jump_bytecode_offset =
      iterator_.GetUnsignedImmediateOperand(0);
  const int32_t loop_offset = iterator_.GetImmediateOperand(1);
  const FeedbackSlot feedback_slot = iterator_.GetSlotOperand(2);
  int target = iterator_.GetJumpTargetOffset();

  if (relative_jump_bytecode_offset > 0) {
    AddNewNode<ReduceInterruptBudget>({}, relative_jump_bytecode_offset);
  }
  BasicBlock* block =
      target == iterator_.current_offset()
          ? FinishBlock<JumpLoop>(next_offset(), {}, &jump_targets_[target],
                                  loop_offset, feedback_slot)
          : FinishBlock<JumpLoop>(next_offset(), {},
                                  jump_targets_[target].block_ptr(),
                                  loop_offset, feedback_slot);

  merge_states_[target]->MergeLoop(*compilation_unit_,
                                   current_interpreter_frame_, block, target);
  block->set_predecessor_id(merge_states_[target]->predecessor_count() - 1);
}
void MaglevGraphBuilder::VisitJump() {
  const uint32_t relative_jump_bytecode_offset =
      iterator_.GetUnsignedImmediateOperand(0);
  if (relative_jump_bytecode_offset > 0) {
    AddNewNode<IncreaseInterruptBudget>({}, relative_jump_bytecode_offset);
  }
  BasicBlock* block = FinishBlock<Jump>(
      next_offset(), {}, &jump_targets_[iterator_.GetJumpTargetOffset()]);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
  DCHECK_LT(next_offset(), bytecode().length());
}
MAGLEV_UNIMPLEMENTED_BYTECODE(JumpConstant)
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
    merge_states_[target] = zone()->New<MergePointInterpreterFrameState>(
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
    // If this merge is the last one which kills a loop merge, remove that merge
    // state.
    if (merge_states_[target]->is_unreachable_loop()) {
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
    merge_states_[target] = zone()->New<MergePointInterpreterFrameState>(
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

void MaglevGraphBuilder::BuildBranchIfTrue(ValueNode* node, int true_target,
                                           int false_target) {
  BasicBlock* block = FinishBlock<BranchIfTrue>(next_offset(), {node},
                                                &jump_targets_[true_target],
                                                &jump_targets_[false_target]);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
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
MAGLEV_UNIMPLEMENTED_BYTECODE(JumpIfNull)
MAGLEV_UNIMPLEMENTED_BYTECODE(JumpIfNotNull)
MAGLEV_UNIMPLEMENTED_BYTECODE(JumpIfUndefined)
MAGLEV_UNIMPLEMENTED_BYTECODE(JumpIfNotUndefined)
MAGLEV_UNIMPLEMENTED_BYTECODE(JumpIfUndefinedOrNull)
MAGLEV_UNIMPLEMENTED_BYTECODE(JumpIfJSReceiver)
MAGLEV_UNIMPLEMENTED_BYTECODE(SwitchOnSmiNoFeedback)
MAGLEV_UNIMPLEMENTED_BYTECODE(ForInEnumerate)
MAGLEV_UNIMPLEMENTED_BYTECODE(ForInPrepare)
MAGLEV_UNIMPLEMENTED_BYTECODE(ForInContinue)
MAGLEV_UNIMPLEMENTED_BYTECODE(ForInNext)
MAGLEV_UNIMPLEMENTED_BYTECODE(ForInStep)
MAGLEV_UNIMPLEMENTED_BYTECODE(SetPendingMessage)
MAGLEV_UNIMPLEMENTED_BYTECODE(Throw)
MAGLEV_UNIMPLEMENTED_BYTECODE(ReThrow)
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
MAGLEV_UNIMPLEMENTED_BYTECODE(ThrowReferenceErrorIfHole)
MAGLEV_UNIMPLEMENTED_BYTECODE(ThrowSuperNotCalledIfHole)
MAGLEV_UNIMPLEMENTED_BYTECODE(ThrowSuperAlreadyCalledIfNotHole)
MAGLEV_UNIMPLEMENTED_BYTECODE(ThrowIfNotSuperConstructor)
MAGLEV_UNIMPLEMENTED_BYTECODE(SwitchOnGeneratorState)
MAGLEV_UNIMPLEMENTED_BYTECODE(SuspendGenerator)
MAGLEV_UNIMPLEMENTED_BYTECODE(ResumeGenerator)
MAGLEV_UNIMPLEMENTED_BYTECODE(GetIterator)
MAGLEV_UNIMPLEMENTED_BYTECODE(Debugger)
MAGLEV_UNIMPLEMENTED_BYTECODE(IncBlockCounter)
MAGLEV_UNIMPLEMENTED_BYTECODE(Abort)

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
