// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-builder.h"

#include "src/base/optional.h"
#include "src/base/v8-fallthrough.h"
#include "src/builtins/builtins-constructor.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/common/globals.h"
#include "src/compiler/access-info.h"
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
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/elements-kind.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/property-cell.h"
#include "src/objects/property-details.h"
#include "src/objects/slots-inl.h"

namespace v8::internal::maglev {

namespace {

ValueNode* TryGetParentContext(ValueNode* node) {
  if (CreateFunctionContext* n = node->TryCast<CreateFunctionContext>()) {
    return n->context().node();
  }

  if (CallRuntime* n = node->TryCast<CallRuntime>()) {
    switch (n->function_id()) {
      case Runtime::kPushBlockContext:
      case Runtime::kPushCatchContext:
      case Runtime::kNewFunctionContext:
        return n->context().node();
      default:
        break;
    }
  }

  return nullptr;
}

// Attempts to walk up the context chain through the graph in order to reduce
// depth and thus the number of runtime loads.
void MinimizeContextChainDepth(ValueNode** context, size_t* depth) {
  while (*depth > 0) {
    ValueNode* parent_context = TryGetParentContext(*context);
    if (parent_context == nullptr) return;
    *context = parent_context;
    (*depth)--;
  }
}

class FunctionContextSpecialization final : public AllStatic {
 public:
  static base::Optional<compiler::ContextRef> TryToRef(
      const MaglevCompilationUnit* unit, ValueNode* context, size_t* depth) {
    DCHECK(unit->info()->specialize_to_function_context());
    base::Optional<compiler::ContextRef> ref;
    if (InitialValue* n = context->TryCast<InitialValue>()) {
      if (n->source().is_current_context()) {
        ref = unit->function().context();
      }
    } else if (Constant* n = context->TryCast<Constant>()) {
      ref = n->ref().AsContext();
    }
    if (!ref.has_value()) return {};
    return ref->previous(depth);
  }
};

}  // namespace

MaglevGraphBuilder::MaglevGraphBuilder(LocalIsolate* local_isolate,
                                       MaglevCompilationUnit* compilation_unit,
                                       Graph* graph, MaglevGraphBuilder* parent)
    : local_isolate_(local_isolate),
      compilation_unit_(compilation_unit),
      parent_(parent),
      graph_(graph),
      bytecode_analysis_(bytecode().object(), zone(), BytecodeOffset::None(),
                         true),
      iterator_(bytecode().object()),
      source_position_iterator_(bytecode().SourcePositionTable()),
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
}

BasicBlock* MaglevGraphBuilder::EndPrologue() {
  BasicBlock* first_block = FinishBlock<Jump>({}, &jump_targets_[0]);
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
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "- Creating loop merge state at @" << offset << std::endl;
    }
    merge_states_[offset] = MergePointInterpreterFrameState::NewForLoop(
        current_interpreter_frame_, *compilation_unit_, offset,
        NumPredecessors(offset), liveness, &loop_info);
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
      if (v8_flags.trace_maglev_graph_building) {
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
    case Operation::kModulus:
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
  V(Modulus, Int32ModulusWithOverflow, {})  \
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
ValueNode* MaglevGraphBuilder::TryFoldInt32BinaryOperation(ValueNode* left,
                                                           ValueNode* right) {
  switch (kOperation) {
    case Operation::kModulus:
      // x % x = 0
      if (right == left) return GetInt32Constant(0);
      break;
    default:
      // TODO(victorgomes): Implement more folds.
      break;
  }
  return nullptr;
}

template <Operation kOperation>
ValueNode* MaglevGraphBuilder::TryFoldInt32BinaryOperation(ValueNode* left,
                                                           int right) {
  switch (kOperation) {
    case Operation::kModulus:
      // x % 1 = 0
      // x % -1 = 0
      if (right == 1 || right == -1) return GetInt32Constant(0);
      // TODO(victorgomes): We can emit faster mod operation if {right} is power
      // of 2, unfortunately we need to know if {left} is negative or not.
      // Maybe emit a Int32ModulusRightIsPowerOf2?
      break;
    default:
      // TODO(victorgomes): Implement more folds.
      break;
  }
  return nullptr;
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildInt32BinaryOperationNode() {
  // TODO(v8:7700): Do constant folding.
  ValueNode* left = LoadRegisterInt32(0);
  ValueNode* right = GetAccumulatorInt32();

  if (ValueNode* result =
          TryFoldInt32BinaryOperation<kOperation>(left, right)) {
    SetAccumulator(result);
    return;
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
  if (ValueNode* result =
          TryFoldInt32BinaryOperation<kOperation>(left, constant)) {
    SetAccumulator(result);
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
  ValueNode* left = LoadRegisterFloat64(0);
  ValueNode* right = GetAccumulatorFloat64();

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
      {left, right}, operation, &jump_targets_[true_offset],
      &jump_targets_[false_offset]);
  if (true_offset == iterator_.GetJumpTargetOffset()) {
    block->control_node()
        ->Cast<BranchControlNode>()
        ->set_true_interrupt_correction(
            iterator_.GetRelativeJumpTargetOffset());
  } else {
    block->control_node()
        ->Cast<BranchControlNode>()
        ->set_false_interrupt_correction(
            iterator_.GetRelativeJumpTargetOffset());
  }
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
  StartFallthroughBlock(next_offset(), block);
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
        ValueNode* left = LoadRegisterInt32(0);
        ValueNode* right = GetAccumulatorInt32();

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
        ValueNode* left = LoadRegisterFloat64(0);
        ValueNode* right = GetAccumulatorFloat64();

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
        left = right = GetInternalizedString(iterator_.GetRegisterOperand(0));
        current_interpreter_frame_.set(
            interpreter::Register::virtual_accumulator(), left);
      } else {
        left = GetInternalizedString(iterator_.GetRegisterOperand(0));
        right =
            GetInternalizedString(interpreter::Register::virtual_accumulator());
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
      ValueNode* left = LoadRegisterTagged(0);
      ValueNode* right = GetAccumulatorTagged();
      BuildCheckSymbol(left);
      BuildCheckSymbol(right);
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

bool MaglevGraphBuilder::TrySpecializeLoadContextSlotToFunctionContext(
    ValueNode** context, size_t* depth, int slot_index,
    ContextSlotMutability slot_mutability) {
  DCHECK(compilation_unit_->info()->specialize_to_function_context());

  size_t new_depth = *depth;
  base::Optional<compiler::ContextRef> maybe_context_ref =
      FunctionContextSpecialization::TryToRef(compilation_unit_, *context,
                                              &new_depth);
  if (!maybe_context_ref.has_value()) return false;

  compiler::ContextRef context_ref = maybe_context_ref.value();
  if (slot_mutability == kMutable || new_depth != 0) {
    *depth = new_depth;
    *context = GetConstant(context_ref);
    return false;
  }

  base::Optional<compiler::ObjectRef> maybe_slot_value =
      context_ref.get(slot_index);
  if (!maybe_slot_value.has_value()) {
    *depth = new_depth;
    *context = GetConstant(context_ref);
    return false;
  }

  compiler::ObjectRef slot_value = maybe_slot_value.value();
  if (slot_value.IsHeapObject()) {
    // Even though the context slot is immutable, the context might have escaped
    // before the function to which it belongs has initialized the slot.  We
    // must be conservative and check if the value in the slot is currently the
    // hole or undefined. Only if it is neither of these, can we be sure that it
    // won't change anymore.
    //
    // See also: JSContextSpecialization::ReduceJSLoadContext.
    compiler::OddballType oddball_type =
        slot_value.AsHeapObject().map().oddball_type();
    if (oddball_type == compiler::OddballType::kUndefined ||
        oddball_type == compiler::OddballType::kHole) {
      *depth = new_depth;
      *context = GetConstant(context_ref);
      return false;
    }
  }

  // Fold the load of the immutable slot.

  SetAccumulator(GetConstant(slot_value));
  return true;
}

ValueNode* MaglevGraphBuilder::LoadAndCacheContextSlot(
    ValueNode* context, int offset, ContextSlotMutability slot_mutability) {
  ValueNode*& cached_value =
      slot_mutability == ContextSlotMutability::kMutable
          ? known_node_aspects().loaded_context_slots[{context, offset}]
          : known_node_aspects().loaded_context_constants[{context, offset}];
  if (cached_value) {
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "  * Reusing cached context slot "
                << PrintNodeLabel(graph_labeller(), context) << "[" << offset
                << "]: " << PrintNode(graph_labeller(), cached_value)
                << std::endl;
    }
    return cached_value;
  }
  return cached_value = AddNewNode<LoadTaggedField>({context}, offset);
}

void MaglevGraphBuilder::BuildLoadContextSlot(
    ValueNode* context, size_t depth, int slot_index,
    ContextSlotMutability slot_mutability) {
  MinimizeContextChainDepth(&context, &depth);

  if (compilation_unit_->info()->specialize_to_function_context() &&
      TrySpecializeLoadContextSlotToFunctionContext(
          &context, &depth, slot_index, slot_mutability)) {
    return;  // Our work here is done.
  }

  for (size_t i = 0; i < depth; ++i) {
    context = LoadAndCacheContextSlot(
        context, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX),
        kImmutable);
  }

  // Always load the slot here as if it were mutable. Immutable slots have a
  // narrow range of mutability if the context escapes before the slot is
  // initialized, so we can't safely assume that the load can be cached in case
  // it's a load before initialization (e.g. var a = a + 42).
  current_interpreter_frame_.set_accumulator(LoadAndCacheContextSlot(
      context, Context::OffsetOfElementAt(slot_index), kMutable));
}

void MaglevGraphBuilder::VisitLdaContextSlot() {
  ValueNode* context = LoadRegisterTagged(0);
  int slot_index = iterator_.GetIndexOperand(1);
  size_t depth = iterator_.GetUnsignedImmediateOperand(2);
  BuildLoadContextSlot(context, depth, slot_index, kMutable);
}
void MaglevGraphBuilder::VisitLdaImmutableContextSlot() {
  ValueNode* context = LoadRegisterTagged(0);
  int slot_index = iterator_.GetIndexOperand(1);
  size_t depth = iterator_.GetUnsignedImmediateOperand(2);
  BuildLoadContextSlot(context, depth, slot_index, kImmutable);
}
void MaglevGraphBuilder::VisitLdaCurrentContextSlot() {
  ValueNode* context = GetContext();
  int slot_index = iterator_.GetIndexOperand(0);
  BuildLoadContextSlot(context, 0, slot_index, kMutable);
}
void MaglevGraphBuilder::VisitLdaImmutableCurrentContextSlot() {
  ValueNode* context = GetContext();
  int slot_index = iterator_.GetIndexOperand(0);
  BuildLoadContextSlot(context, 0, slot_index, kImmutable);
}

void MaglevGraphBuilder::VisitStaContextSlot() {
  ValueNode* context = LoadRegisterTagged(0);
  int slot_index = iterator_.GetIndexOperand(1);
  size_t depth = iterator_.GetUnsignedImmediateOperand(2);

  MinimizeContextChainDepth(&context, &depth);

  if (compilation_unit_->info()->specialize_to_function_context()) {
    base::Optional<compiler::ContextRef> maybe_ref =
        FunctionContextSpecialization::TryToRef(compilation_unit_, context,
                                                &depth);
    if (maybe_ref.has_value()) {
      context = GetConstant(maybe_ref.value());
    }
  }

  for (size_t i = 0; i < depth; ++i) {
    context = LoadAndCacheContextSlot(
        context, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX),
        kImmutable);
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
  if (TryBuildCompareOperation<BranchIfReferenceCompare>(
          Operation::kStrictEqual, lhs, rhs)) {
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

bool MaglevGraphBuilder::TryBuildScriptContextConstantAccess(
    const compiler::GlobalAccessFeedback& global_access_feedback) {
  if (!global_access_feedback.immutable()) return false;

  base::Optional<compiler::ObjectRef> maybe_slot_value =
      global_access_feedback.script_context().get(
          global_access_feedback.slot_index());
  if (!maybe_slot_value) return false;

  SetAccumulator(GetConstant(maybe_slot_value.value()));
  return true;
}

bool MaglevGraphBuilder::TryBuildScriptContextAccess(
    const compiler::GlobalAccessFeedback& global_access_feedback) {
  if (!global_access_feedback.IsScriptContextSlot()) return false;
  if (TryBuildScriptContextConstantAccess(global_access_feedback)) return true;

  auto script_context = GetConstant(global_access_feedback.script_context());
  current_interpreter_frame_.set_accumulator(LoadAndCacheContextSlot(
      script_context,
      Context::OffsetOfElementAt(global_access_feedback.slot_index()),
      global_access_feedback.immutable() ? kImmutable : kMutable));
  return true;
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

namespace {
NodeType StaticTypeForNode(ValueNode* node) {
  DCHECK(node->is_tagged());
  switch (node->opcode()) {
    case Opcode::kCheckedSmiTag:
    case Opcode::kSmiConstant:
      return NodeType::kSmi;
    case Opcode::kConstant: {
      compiler::HeapObjectRef ref = node->Cast<Constant>()->object();
      if (ref.IsString()) {
        return NodeType::kString;
      } else if (ref.IsSymbol()) {
        return NodeType::kSymbol;
      } else if (ref.IsHeapNumber()) {
        return NodeType::kHeapNumber;
      } else if (ref.IsJSReceiver()) {
        return NodeType::kJSReceiverWithKnownMap;
      }
      return NodeType::kHeapObjectWithKnownMap;
    }
    case Opcode::kToNumberOrNumeric:
      if (node->Cast<ToNumberOrNumeric>()->mode() ==
          Object::Conversion::kToNumber) {
        return NodeType::kNumber;
      }
      // TODO(verwaest): Check what we need here.
      return NodeType::kUnknown;
    case Opcode::kToString:
      return NodeType::kString;
    case Opcode::kToObject:
      return NodeType::kJSReceiver;
    case Opcode::kToName:
      return NodeType::kName;
    default:
      return NodeType::kUnknown;
  }
}
}  // namespace

NodeInfo* MaglevGraphBuilder::CreateInfoIfNot(ValueNode* node, NodeType type) {
  NodeType static_type = StaticTypeForNode(node);
  if (NodeTypeIs(static_type, type)) return nullptr;
  NodeInfo* known_info = known_node_aspects().GetOrCreateInfoFor(node);
  if (NodeTypeIs(known_info->type, type)) return nullptr;
  known_info->type = CombineType(known_info->type, static_type);
  return known_info;
}

bool MaglevGraphBuilder::CheckType(ValueNode* node, NodeType type) {
  if (NodeTypeIs(StaticTypeForNode(node), type)) return true;
  auto it = known_node_aspects().FindInfo(node);
  if (!known_node_aspects().IsValid(it)) return false;
  return NodeTypeIs(it->second.type, type);
}

bool MaglevGraphBuilder::EnsureType(ValueNode* node, NodeType type,
                                    NodeType* old) {
  NodeInfo* known_info = CreateInfoIfNot(node, type);
  if (known_info == nullptr) return true;
  if (old != nullptr) *old = known_info->type;
  known_info->type = CombineType(known_info->type, type);
  return false;
}

ValueNode* MaglevGraphBuilder::BuildSmiUntag(ValueNode* node) {
  if (EnsureType(node, NodeType::kSmi)) {
    return AddNewNode<UnsafeSmiUntag>({node});
  } else {
    return AddNewNode<CheckedSmiUntag>({node});
  }
}

void MaglevGraphBuilder::BuildCheckSmi(ValueNode* object) {
  if (EnsureType(object, NodeType::kSmi)) return;
  AddNewNode<CheckSmi>({object});
}

void MaglevGraphBuilder::BuildCheckHeapObject(ValueNode* object) {
  if (EnsureType(object, NodeType::kAnyHeapObject)) return;
  AddNewNode<CheckHeapObject>({object});
}

namespace {
CheckType GetCheckType(NodeType type) {
  return NodeTypeIs(type, NodeType::kAnyHeapObject)
             ? CheckType::kOmitHeapObjectCheck
             : CheckType::kCheckHeapObject;
}
}  // namespace

void MaglevGraphBuilder::BuildCheckString(ValueNode* object) {
  NodeType known_type;
  if (EnsureType(object, NodeType::kString, &known_type)) return;
  AddNewNode<CheckString>({object}, GetCheckType(known_type));
}

void MaglevGraphBuilder::BuildCheckNumber(ValueNode* object) {
  if (EnsureType(object, NodeType::kNumber)) return;
  AddNewNode<CheckNumber>({object}, Object::Conversion::kToNumber);
}

void MaglevGraphBuilder::BuildCheckSymbol(ValueNode* object) {
  NodeType known_type;
  if (EnsureType(object, NodeType::kSymbol, &known_type)) return;
  AddNewNode<CheckSymbol>({object}, GetCheckType(known_type));
}

namespace {

class KnownMapsMerger {
 public:
  explicit KnownMapsMerger(compiler::JSHeapBroker* broker, ValueNode* object,
                           KnownNodeAspects& known_node_aspects,
                           ZoneVector<compiler::MapRef> const& maps)
      : broker_(broker),
        maps_(maps),
        known_maps_are_subset_of_maps_(true),
        emit_check_with_migration_(false) {
    // A non-value value here means the universal set, i.e., we don't know
    // anything about the possible maps of the object.
    base::Optional<ZoneHandleSet<Map>> known_stable_map_set =
        GetKnownMapSet(object, known_node_aspects.stable_maps);
    base::Optional<ZoneHandleSet<Map>> known_unstable_map_set =
        GetKnownMapSet(object, known_node_aspects.unstable_maps);

    IntersectKnownMaps(known_stable_map_set, true);
    IntersectKnownMaps(known_unstable_map_set, false);

    // Update known maps.
    known_node_aspects.stable_maps[object] = stable_map_set_;
    known_node_aspects.unstable_maps[object] = unstable_map_set_;
  }

  bool known_maps_are_subset_of_maps() const {
    return known_maps_are_subset_of_maps_;
  }
  bool emit_check_with_migration() const { return emit_check_with_migration_; }

  ZoneHandleSet<Map> intersect_set() const {
    ZoneHandleSet<Map> map_set;
    map_set.Union(stable_map_set_, zone());
    map_set.Union(unstable_map_set_, zone());
    return map_set;
  }

  NodeType node_type() const { return node_type_; }

 private:
  compiler::JSHeapBroker* broker_;
  ZoneVector<compiler::MapRef> const& maps_;
  bool known_maps_are_subset_of_maps_;
  bool emit_check_with_migration_;
  ZoneHandleSet<Map> stable_map_set_;
  ZoneHandleSet<Map> unstable_map_set_;
  NodeType node_type_ = NodeType::kJSReceiverWithKnownMap;

  Zone* zone() const { return broker_->zone(); }

  base::Optional<ZoneHandleSet<Map>> GetKnownMapSet(
      ValueNode* object,
      const ZoneMap<ValueNode*, ZoneHandleSet<Map>>& map_of_map_set) {
    auto it = map_of_map_set.find(object);
    if (it == map_of_map_set.end()) {
      return {};
    }
    return it->second;
  }

  base::Optional<compiler::MapRef> GetMapRefFromMaps(Handle<Map> handle) {
    auto it =
        std::find_if(maps_.begin(), maps_.end(), [&](compiler::MapRef map_ref) {
          return map_ref.object().is_identical_to(handle);
        });
    if (it == maps_.end()) return {};
    return *it;
  }

  void InsertMap(compiler::MapRef map, bool add_dependency) {
    if (map.is_migration_target()) {
      emit_check_with_migration_ = true;
    }
    if (!map.IsJSReceiverMap()) node_type_ = NodeType::kHeapObjectWithKnownMap;
    if (map.is_stable()) {
      // TODO(victorgomes): Add a DCHECK_SLOW that checks if the map already
      // exists in the CompilationDependencySet for the else branch.
      if (add_dependency) {
        broker_->dependencies()->DependOnStableMap(map);
      }
      stable_map_set_.insert(map.object(), zone());
    } else {
      unstable_map_set_.insert(map.object(), zone());
    }
  }

  void IntersectKnownMaps(base::Optional<ZoneHandleSet<Map>>& known_maps,
                          bool is_set_with_stable_maps) {
    if (known_maps.has_value()) {
      // TODO(v8:7700): Make intersection non-quadratic.
      for (Handle<Map> handle : *known_maps) {
        auto map = GetMapRefFromMaps(handle);
        if (map.has_value()) {
          InsertMap(*map, false);
        } else {
          known_maps_are_subset_of_maps_ = false;
        }
      }
    } else {
      // Intersect with the universal set.
      known_maps_are_subset_of_maps_ = false;
      for (compiler::MapRef map : maps_) {
        if (map.is_stable() == is_set_with_stable_maps) {
          InsertMap(map, true);
        }
      }
    }
  }
};

}  // namespace

void MaglevGraphBuilder::BuildCheckMaps(
    ValueNode* object, ZoneVector<compiler::MapRef> const& maps) {
  // TODO(verwaest): Support other objects with possible known stable maps as
  // well.
  if (object->Is<Constant>()) {
    // For constants with stable maps that match one of the desired maps, we
    // don't need to emit a map check, and can use the dependency -- we
    // can't do this for unstable maps because the constant could migrate
    // during compilation.
    compiler::MapRef constant_map = object->Cast<Constant>()->object().map();
    if (std::find(maps.begin(), maps.end(), constant_map) != maps.end()) {
      if (constant_map.is_stable()) {
        broker()->dependencies()->DependOnStableMap(constant_map);
        return;
      }
      // TODO(verwaest): Reduce maps to the constant map.
    } else {
      // TODO(leszeks): Insert an unconditional deopt if the constant map
      // doesn't match the required map.
    }
  }

  NodeInfo* known_info = known_node_aspects().GetOrCreateInfoFor(object);
  known_info->type = CombineType(known_info->type, StaticTypeForNode(object));

  // Calculates if known maps are a subset of maps, their map intersection and
  // whether we should emit check with migration.
  KnownMapsMerger merger(broker(), object, known_node_aspects(), maps);

  // If the known maps are the subset of the maps to check, we are done.
  if (merger.known_maps_are_subset_of_maps()) {
    DCHECK(NodeTypeIs(known_info->type, NodeType::kHeapObjectWithKnownMap));
    return;
  }

  // TODO(v8:7700): Insert an unconditional deopt here if intersect map sets are
  // empty.

  // TODO(v8:7700): Check if the {maps} - {known_maps} size is smaller than
  // {maps} \intersect {known_maps}, we can emit CheckNotMaps instead.

  // Emit checks.
  if (merger.emit_check_with_migration()) {
    AddNewNode<CheckMapsWithMigration>({object}, merger.intersect_set(),
                                       GetCheckType(known_info->type));
  } else {
    AddNewNode<CheckMaps>({object}, merger.intersect_set(),
                          GetCheckType(known_info->type));
  }
  known_info->type = merger.node_type();
}

bool MaglevGraphBuilder::TryFoldLoadDictPrototypeConstant(
    compiler::PropertyAccessInfo access_info) {
  DCHECK(V8_DICT_PROPERTY_CONST_TRACKING_BOOL);
  DCHECK(access_info.IsDictionaryProtoDataConstant());
  DCHECK(access_info.holder().has_value());

  base::Optional<compiler::ObjectRef> constant =
      access_info.holder()->GetOwnDictionaryProperty(
          access_info.dictionary_index(), broker()->dependencies());
  if (!constant.has_value()) return false;

  for (compiler::MapRef map : access_info.lookup_start_object_maps()) {
    Handle<Map> map_handle = map.object();
    // Non-JSReceivers that passed AccessInfoFactory::ComputePropertyAccessInfo
    // must have different lookup start map.
    if (!map_handle->IsJSReceiverMap()) {
      // Perform the implicit ToObject for primitives here.
      // Implemented according to ES6 section 7.3.2 GetV (V, P).
      JSFunction constructor =
          Map::GetConstructorFunction(
              *map_handle, *broker()->target_native_context().object())
              .value();
      // {constructor.initial_map()} is loaded/stored with acquire-release
      // semantics for constructors.
      map = MakeRefAssumeMemoryFence(broker(), constructor.initial_map());
      DCHECK(map.object()->IsJSObjectMap());
    }
    broker()->dependencies()->DependOnConstantInDictionaryPrototypeChain(
        map, access_info.name(), constant.value(), PropertyKind::kData);
  }

  SetAccumulator(GetConstant(constant.value()));
  return true;
}

bool MaglevGraphBuilder::TryFoldLoadConstantDataField(
    compiler::PropertyAccessInfo access_info, ValueNode* lookup_start_object) {
  if (!access_info.IsFastDataConstant()) return false;
  base::Optional<compiler::JSObjectRef> source;
  if (access_info.holder().has_value()) {
    source = access_info.holder();
  } else if (Constant* n = lookup_start_object->TryCast<Constant>()) {
    if (!n->ref().IsJSObject()) return false;
    source = n->ref().AsJSObject();
  } else {
    return false;
  }
  base::Optional<compiler::ObjectRef> constant =
      source.value().GetOwnFastDataProperty(access_info.field_representation(),
                                            access_info.field_index(),
                                            broker()->dependencies());
  if (!constant.has_value()) return false;
  SetAccumulator(GetConstant(constant.value()));
  return true;
}

bool MaglevGraphBuilder::TryBuildPropertyGetterCall(
    compiler::PropertyAccessInfo access_info, ValueNode* receiver,
    ValueNode* lookup_start_object) {
  compiler::ObjectRef constant = access_info.constant().value();

  if (access_info.IsDictionaryProtoAccessorConstant()) {
    // For fast mode holders we recorded dependencies in BuildPropertyLoad.
    for (const compiler::MapRef map : access_info.lookup_start_object_maps()) {
      broker()->dependencies()->DependOnConstantInDictionaryPrototypeChain(
          map, access_info.name(), constant, PropertyKind::kAccessor);
    }
  }

  // Introduce the call to the getter function.
  if (constant.IsJSFunction()) {
    ConvertReceiverMode receiver_mode =
        receiver == lookup_start_object
            ? ConvertReceiverMode::kNotNullOrUndefined
            : ConvertReceiverMode::kAny;
    Call* call = CreateNewNode<Call>(Call::kFixedInputCount + 1, receiver_mode,
                                     Call::TargetType::kJSFunction,
                                     compiler::FeedbackSource(),
                                     GetConstant(constant), GetContext());
    call->set_arg(0, receiver);
    SetAccumulator(AddNode(call));
    return true;
  } else {
    // TODO(victorgomes): API calls.
    return false;
  }
}

bool MaglevGraphBuilder::TryBuildPropertySetterCall(
    compiler::PropertyAccessInfo access_info, ValueNode* receiver,
    ValueNode* value) {
  compiler::ObjectRef constant = access_info.constant().value();
  if (constant.IsJSFunction()) {
    Call* call = CreateNewNode<Call>(
        Call::kFixedInputCount + 2, ConvertReceiverMode::kNotNullOrUndefined,
        Call::TargetType::kJSFunction, compiler::FeedbackSource(),
        GetConstant(constant), GetContext());
    call->set_arg(0, receiver);
    call->set_arg(1, value);
    SetAccumulator(AddNode(call));
    return true;
  } else {
    // TODO(victorgomes): API calls.
    return false;
  }
}

void MaglevGraphBuilder::BuildLoadField(
    compiler::PropertyAccessInfo access_info, ValueNode* lookup_start_object) {
  if (TryFoldLoadConstantDataField(access_info, lookup_start_object)) return;

  // Resolve property holder.
  ValueNode* load_source;
  if (access_info.holder().has_value()) {
    load_source = GetConstant(access_info.holder().value());
  } else {
    load_source = lookup_start_object;
  }

  FieldIndex field_index = access_info.field_index();
  if (!field_index.is_inobject()) {
    // The field is in the property array, first load it from there.
    load_source = AddNewNode<LoadTaggedField>(
        {load_source}, JSReceiver::kPropertiesOrHashOffset);
  }

  // Do the load.
  if (field_index.is_double()) {
    SetAccumulator(
        AddNewNode<LoadDoubleField>({load_source}, field_index.offset()));
  } else {
    ValueNode* value =
        AddNewNode<LoadTaggedField>({load_source}, field_index.offset());
    SetAccumulator(value);
    // Insert stable field information if present.
    if (access_info.field_representation().IsSmi()) {
      NodeInfo* known_info = known_node_aspects().GetOrCreateInfoFor(value);
      known_info->type = NodeType::kSmi;
    } else if (access_info.field_representation().IsHeapObject()) {
      NodeInfo* known_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (access_info.field_map().has_value() &&
          access_info.field_map().value().is_stable()) {
        DCHECK(access_info.field_map().value().IsJSReceiverMap());
        known_info->type = NodeType::kJSReceiverWithKnownMap;
        auto map = access_info.field_map().value();
        ZoneHandleSet<Map> stable_maps(map.object());
        ZoneHandleSet<Map> unstable_maps;
        known_node_aspects().stable_maps.emplace(value, stable_maps);
        known_node_aspects().unstable_maps.emplace(value, unstable_maps);
        broker()->dependencies()->DependOnStableMap(map);
      } else {
        known_info->type = NodeType::kAnyHeapObject;
      }
    }
  }
}

bool MaglevGraphBuilder::TryBuildStoreField(
    compiler::PropertyAccessInfo access_info, ValueNode* receiver) {
  FieldIndex field_index = access_info.field_index();
  Representation field_representation = access_info.field_representation();

  if (access_info.HasTransitionMap()) {
    compiler::MapRef transition = access_info.transition_map().value();
    compiler::MapRef original_map = transition.GetBackPointer().AsMap();
    // TODO(verwaest): Support growing backing stores.
    if (original_map.UnusedPropertyFields() == 0) return false;
  } else if (access_info.IsFastDataConstant()) {
    return false;
  }

  ValueNode* store_target;
  if (field_index.is_inobject()) {
    store_target = receiver;
  } else {
    // The field is in the property array, first load it from there.
    store_target = AddNewNode<LoadTaggedField>(
        {receiver}, JSReceiver::kPropertiesOrHashOffset);
  }

  ValueNode* value;
  if (field_representation.IsDouble()) {
    value = GetAccumulatorFloat64();
    if (access_info.HasTransitionMap()) {
      // Allocate the mutable double box owned by the field.
      value = AddNewNode<Float64Box>({value});
    }
  } else {
    value = GetAccumulatorTagged();
    if (field_representation.IsSmi()) {
      BuildCheckSmi(value);
    } else if (field_representation.IsHeapObject()) {
      // Emit a map check for the field type, if needed, otherwise just a
      // HeapObject check.
      if (access_info.field_map().has_value()) {
        ZoneVector<compiler::MapRef> maps({access_info.field_map().value()},
                                          zone());
        BuildCheckMaps(value, maps);
      } else {
        BuildCheckHeapObject(value);
      }
    }
  }

  if (field_representation.IsSmi()) {
    AddNewNode<StoreTaggedFieldNoWriteBarrier>({store_target, value},
                                               field_index.offset());
  } else if (value->use_double_register()) {
    DCHECK(field_representation.IsDouble());
    DCHECK(!access_info.HasTransitionMap());
    AddNewNode<StoreDoubleField>({store_target, value}, field_index.offset());
  } else {
    DCHECK(field_representation.IsHeapObject() ||
           field_representation.IsTagged() ||
           (field_representation.IsDouble() && access_info.HasTransitionMap()));
    AddNewNode<StoreTaggedFieldWithWriteBarrier>({store_target, value},
                                                 field_index.offset());
  }

  if (access_info.HasTransitionMap()) {
    compiler::MapRef transition = access_info.transition_map().value();
    AddNewNode<StoreMap>({receiver}, transition);
    NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(receiver);
    DCHECK(transition.IsJSReceiverMap());
    node_info->type = NodeType::kJSReceiverWithKnownMap;
    if (transition.is_stable()) {
      ZoneHandleSet<Map> stable_maps(transition.object());
      ZoneHandleSet<Map> unstable_maps;
      known_node_aspects().stable_maps.emplace(receiver, stable_maps);
      known_node_aspects().unstable_maps.emplace(receiver, unstable_maps);
      broker()->dependencies()->DependOnStableMap(transition);
    } else {
      ZoneHandleSet<Map> stable_maps;
      ZoneHandleSet<Map> unstable_maps(transition.object());
      known_node_aspects().stable_maps.emplace(receiver, stable_maps);
      known_node_aspects().unstable_maps.emplace(receiver, unstable_maps);
    }
  }

  return true;
}

bool MaglevGraphBuilder::TryBuildPropertyLoad(
    ValueNode* receiver, ValueNode* lookup_start_object, compiler::NameRef name,
    compiler::PropertyAccessInfo const& access_info) {
  if (access_info.holder().has_value() && !access_info.HasDictionaryHolder()) {
    broker()->dependencies()->DependOnStablePrototypeChains(
        access_info.lookup_start_object_maps(), kStartAtPrototype,
        access_info.holder().value());
  }

  switch (access_info.kind()) {
    case compiler::PropertyAccessInfo::kInvalid:
      UNREACHABLE();
    case compiler::PropertyAccessInfo::kNotFound:
      SetAccumulator(GetRootConstant(RootIndex::kUndefinedValue));
      return true;
    case compiler::PropertyAccessInfo::kDataField:
    case compiler::PropertyAccessInfo::kFastDataConstant:
      BuildLoadField(access_info, lookup_start_object);
      RecordKnownProperty(lookup_start_object, name,
                          current_interpreter_frame_.accumulator(),
                          access_info.IsFastDataConstant());
      return true;
    case compiler::PropertyAccessInfo::kDictionaryProtoDataConstant:
      return TryFoldLoadDictPrototypeConstant(access_info);
    case compiler::PropertyAccessInfo::kFastAccessorConstant:
    case compiler::PropertyAccessInfo::kDictionaryProtoAccessorConstant:
      return TryBuildPropertyGetterCall(access_info, receiver,
                                        lookup_start_object);
    case compiler::PropertyAccessInfo::kModuleExport: {
      ValueNode* cell = GetConstant(access_info.constant().value().AsCell());
      SetAccumulator(AddNewNode<LoadTaggedField>({cell}, Cell::kValueOffset));
      return true;
    }
    case compiler::PropertyAccessInfo::kStringLength:
      DCHECK_EQ(receiver, lookup_start_object);
      SetAccumulator(AddNewNode<StringLength>({receiver}));
      RecordKnownProperty(lookup_start_object, name,
                          current_interpreter_frame_.accumulator(), true);
      return true;
  }
}

bool MaglevGraphBuilder::TryBuildPropertyStore(
    ValueNode* receiver, compiler::NameRef name,
    compiler::PropertyAccessInfo const& access_info) {
  if (access_info.holder().has_value()) {
    broker()->dependencies()->DependOnStablePrototypeChains(
        access_info.lookup_start_object_maps(), kStartAtPrototype,
        access_info.holder().value());
  }

  if (access_info.IsFastAccessorConstant()) {
    return TryBuildPropertySetterCall(access_info, receiver,
                                      GetAccumulatorTagged());
  } else {
    DCHECK(access_info.IsDataField() || access_info.IsFastDataConstant());
    if (TryBuildStoreField(access_info, receiver)) {
      RecordKnownProperty(receiver, name,
                          current_interpreter_frame_.accumulator(),
                          access_info.IsFastDataConstant());
      return true;
    }
    return false;
  }
}

bool MaglevGraphBuilder::TryBuildPropertyAccess(
    ValueNode* receiver, ValueNode* lookup_start_object, compiler::NameRef name,
    compiler::PropertyAccessInfo const& access_info,
    compiler::AccessMode access_mode) {
  switch (access_mode) {
    case compiler::AccessMode::kLoad:
      return TryBuildPropertyLoad(receiver, lookup_start_object, name,
                                  access_info);
    case compiler::AccessMode::kStore:
    case compiler::AccessMode::kStoreInLiteral:
    case compiler::AccessMode::kDefine:
      DCHECK_EQ(receiver, lookup_start_object);
      return TryBuildPropertyStore(receiver, name, access_info);
    case compiler::AccessMode::kHas:
      // TODO(victorgomes): BuildPropertyTest.
      return false;
  }
}

namespace {
bool HasOnlyStringMaps(ZoneVector<compiler::MapRef> const& maps) {
  for (compiler::MapRef map : maps) {
    if (!map.IsStringMap()) return false;
  }
  return true;
}

bool HasOnlyNumberMaps(ZoneVector<compiler::MapRef> const& maps) {
  for (compiler::MapRef map : maps) {
    if (map.instance_type() != HEAP_NUMBER_TYPE) return false;
  }
  return true;
}

}  // namespace

bool MaglevGraphBuilder::TryBuildNamedAccess(
    ValueNode* receiver, ValueNode* lookup_start_object,
    compiler::NamedAccessFeedback const& feedback,
    compiler::AccessMode access_mode) {
  ZoneVector<compiler::PropertyAccessInfo> access_infos(zone());
  {
    ZoneVector<compiler::PropertyAccessInfo> access_infos_for_feedback(zone());
    if (Constant* n = lookup_start_object->TryCast<Constant>()) {
      compiler::MapRef constant_map = n->object().map();
      compiler::PropertyAccessInfo access_info =
          broker()->GetPropertyAccessInfo(constant_map, feedback.name(),
                                          access_mode,
                                          broker()->dependencies());
      access_infos_for_feedback.push_back(access_info);
    } else {
      for (const compiler::MapRef& map : feedback.maps()) {
        if (map.is_deprecated()) continue;
        compiler::PropertyAccessInfo access_info =
            broker()->GetPropertyAccessInfo(map, feedback.name(), access_mode,
                                            broker()->dependencies());
        access_infos_for_feedback.push_back(access_info);
      }
    }

    compiler::AccessInfoFactory access_info_factory(
        broker(), broker()->dependencies(), zone());
    if (!access_info_factory.FinalizePropertyAccessInfos(
            access_infos_for_feedback, access_mode, &access_infos)) {
      return false;
    }
  }

  // Check for monomorphic case.
  if (access_infos.size() == 1) {
    compiler::PropertyAccessInfo access_info = access_infos.front();
    const ZoneVector<compiler::MapRef>& maps =
        access_info.lookup_start_object_maps();
    if (HasOnlyStringMaps(maps)) {
      // Check for string maps before checking if we need to do an access
      // check. Primitive strings always get the prototype from the native
      // context they're operated on, so they don't need the access check.
      BuildCheckString(lookup_start_object);
    } else if (HasOnlyNumberMaps(maps)) {
      BuildCheckNumber(lookup_start_object);
    } else {
      BuildCheckMaps(lookup_start_object, maps);
    }

    // Generate the actual property access.
    return TryBuildPropertyAccess(receiver, lookup_start_object,
                                  feedback.name(), access_info, access_mode);
  } else {
    // TODO(victorgomes): polymorphic case.
    return false;
  }
}

ValueNode* MaglevGraphBuilder::GetInt32ElementIndex(ValueNode* object) {
  switch (object->properties().value_representation()) {
    case ValueRepresentation::kTagged:
      if (SmiConstant* constant = object->TryCast<SmiConstant>()) {
        return GetInt32Constant(constant->value().value());
      } else if (CheckType(object, NodeType::kSmi)) {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(object);
        if (!node_info->int32_alternative) {
          node_info->int32_alternative = AddNewNode<UnsafeSmiUntag>({object});
        }
        return node_info->int32_alternative;
      } else {
        // TODO(leszeks): Cache this knowledge/converted value somehow on
        // the node info.
        return AddNewNode<CheckedObjectToIndex>({object});
      }
    case ValueRepresentation::kInt32:
      // Already good.
      return object;
    case ValueRepresentation::kFloat64:
      // TODO(leszeks): Pass in the index register (probably the
      // accumulator), so that we can save this truncation on there as a
      // conversion node.
      return AddNewNode<CheckedTruncateFloat64ToInt32>({object});
  }
}

bool MaglevGraphBuilder::TryBuildElementAccessOnString(
    ValueNode* object, ValueNode* index_object,
    compiler::KeyedAccessMode const& keyed_mode) {
  // Strings are immutable and `in` cannot be used on strings
  if (keyed_mode.access_mode() != compiler::AccessMode::kLoad) return false;

  // TODO(victorgomes): Deal with LOAD_IGNORE_OUT_OF_BOUNDS.
  if (keyed_mode.load_mode() == LOAD_IGNORE_OUT_OF_BOUNDS) return false;

  DCHECK_EQ(keyed_mode.load_mode(), STANDARD_LOAD);

  // Ensure that {object} is actually a String.
  BuildCheckString(object);

  ValueNode* length = AddNewNode<StringLength>({object});
  ValueNode* index = GetInt32ElementIndex(index_object);
  AddNewNode<CheckInt32Condition>({index, length}, AssertCondition::kLess,
                                  DeoptimizeReason::kOutOfBounds);

  SetAccumulator(AddNewNode<StringAt>({object, index}));
  return true;
}

bool MaglevGraphBuilder::TryBuildElementAccess(
    ValueNode* object, ValueNode* index_object,
    compiler::ElementAccessFeedback const& feedback) {
  // TODO(victorgomes): Implement other access modes.
  if (feedback.keyed_mode().access_mode() != compiler::AccessMode::kLoad) {
    return false;
  }

  // TODO(leszeks): Add non-deopting bounds check (has to support undefined
  // values).
  if (feedback.keyed_mode().load_mode() != STANDARD_LOAD) {
    return false;
  }

  // TODO(victorgomes): Add fast path for loading from HeapConstant.

  if (!feedback.transition_groups().empty() &&
      feedback.HasOnlyStringMaps(broker())) {
    return TryBuildElementAccessOnString(object, index_object,
                                         feedback.keyed_mode());
  }

  compiler::AccessInfoFactory access_info_factory(
      broker(), broker()->dependencies(), zone());
  ZoneVector<compiler::ElementAccessInfo> access_infos(zone());
  if (!access_info_factory.ComputeElementAccessInfos(feedback, &access_infos) ||
      access_infos.empty()) {
    return false;
  }

  // Check for monomorphic case.
  if (access_infos.size() == 1) {
    compiler::ElementAccessInfo access_info = access_infos.front();

    // TODO(victorgomes): Support elment kind transitions.
    if (access_info.transition_sources().size() != 0) return false;

    // TODO(victorgomes): Support more elements kind.
    ElementsKind elements_kind = access_info.elements_kind();
    if (!IsFastElementsKind(elements_kind)) return false;
    if (IsHoleyElementsKind(elements_kind)) return false;

    const compiler::MapRef& map =
        access_info.lookup_start_object_maps().front();
    if (access_info.lookup_start_object_maps().size() != 1) {
      // TODO(victorgomes): polymorphic case.
      return false;
    }
    BuildCheckMaps(object, access_info.lookup_start_object_maps());

    ValueNode* index = GetInt32ElementIndex(index_object);
    if (map.IsJSArrayMap()) {
      AddNewNode<CheckJSArrayBounds>({object, index});
    } else {
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

  } else {
    // TODO(victorgomes): polymorphic case.
    return false;
  }
}

void MaglevGraphBuilder::RecordKnownProperty(ValueNode* lookup_start_object,
                                             compiler::NameRef name,
                                             ValueNode* value, bool is_const) {
  auto& loaded_properties =
      is_const ? known_node_aspects().loaded_constant_properties
               : known_node_aspects().loaded_properties;
  loaded_properties.emplace(std::make_pair(lookup_start_object, name), value);
}

bool MaglevGraphBuilder::TryReuseKnownPropertyLoad(
    ValueNode* lookup_start_object, compiler::NameRef name) {
  if (auto it = known_node_aspects().loaded_properties.find(
          {lookup_start_object, name});
      it != known_node_aspects().loaded_properties.end()) {
    current_interpreter_frame_.set_accumulator(it->second);
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "  * Reusing non-constant loaded property "
                << PrintNodeLabel(graph_labeller(), it->second) << ": "
                << PrintNode(graph_labeller(), it->second) << std::endl;
    }
    return true;
  }
  if (auto it = known_node_aspects().loaded_constant_properties.find(
          {lookup_start_object, name});
      it != known_node_aspects().loaded_constant_properties.end()) {
    current_interpreter_frame_.set_accumulator(it->second);
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "  * Reusing constant loaded property "
                << PrintNodeLabel(graph_labeller(), it->second) << ": "
                << PrintNode(graph_labeller(), it->second) << std::endl;
    }
    return true;
  }
  return false;
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

    case compiler::ProcessedFeedback::kNamedAccess:
      if (TryReuseKnownPropertyLoad(object, name)) return;
      if (TryBuildNamedAccess(object, object,
                              processed_feedback.AsNamedAccess(),
                              compiler::AccessMode::kLoad)) {
        return;
      }
      break;
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

    case compiler::ProcessedFeedback::kNamedAccess:
      if (TryReuseKnownPropertyLoad(lookup_start_object, name)) return;
      if (TryBuildNamedAccess(receiver, lookup_start_object,
                              processed_feedback.AsNamedAccess(),
                              compiler::AccessMode::kLoad)) {
        return;
      }
      break;

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
      // Get the accumulator without conversion. TryBuildElementAccess
      // will try to pick the best representation.
      ValueNode* index = current_interpreter_frame_.accumulator();
      if (TryBuildElementAccess(object, index,
                                processed_feedback.AsElementAccess())) {
        return;
      }
      break;
    }

    case compiler::ProcessedFeedback::kNamedAccess: {
      ValueNode* key = GetAccumulatorTagged();
      compiler::NameRef name = processed_feedback.AsNamedAccess().name();
      if (!BuildCheckValue(key, name)) return;
      if (TryReuseKnownPropertyLoad(object, name)) return;
      if (TryBuildNamedAccess(object, object,
                              processed_feedback.AsNamedAccess(),
                              compiler::AccessMode::kLoad)) {
        return;
      }
      break;
    }

    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* key = GetAccumulatorTagged();
  SetAccumulator(
      AddNewNode<GetKeyedGeneric>({context, object, key}, feedback_source));
}

void MaglevGraphBuilder::VisitLdaModuleVariable() {
  // LdaModuleVariable <cell_index> <depth>
  int cell_index = iterator_.GetImmediateOperand(0);
  size_t depth = iterator_.GetUnsignedImmediateOperand(1);

  ValueNode* context = GetContext();
  MinimizeContextChainDepth(&context, &depth);

  if (compilation_unit_->info()->specialize_to_function_context()) {
    base::Optional<compiler::ContextRef> maybe_ref =
        FunctionContextSpecialization::TryToRef(compilation_unit_, context,
                                                &depth);
    if (maybe_ref.has_value()) {
      context = GetConstant(maybe_ref.value());
    }
  }

  for (size_t i = 0; i < depth; i++) {
    context = LoadAndCacheContextSlot(
        context, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX),
        kImmutable);
  }
  ValueNode* module = LoadAndCacheContextSlot(
      context, Context::OffsetOfElementAt(Context::EXTENSION_INDEX),
      kImmutable);
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
  size_t depth = iterator_.GetUnsignedImmediateOperand(1);
  MinimizeContextChainDepth(&context, &depth);

  if (compilation_unit_->info()->specialize_to_function_context()) {
    base::Optional<compiler::ContextRef> maybe_ref =
        FunctionContextSpecialization::TryToRef(compilation_unit_, context,
                                                &depth);
    if (maybe_ref.has_value()) {
      context = GetConstant(maybe_ref.value());
    }
  }

  for (size_t i = 0; i < depth; i++) {
    context = LoadAndCacheContextSlot(
        context, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX),
        kImmutable);
  }
  ValueNode* module = LoadAndCacheContextSlot(
      context, Context::OffsetOfElementAt(Context::EXTENSION_INDEX),
      kImmutable);
  ValueNode* exports = AddNewNode<LoadTaggedField>(
      {module}, SourceTextModule::kRegularExportsOffset);
  // The actual array index is (cell_index - 1).
  cell_index -= 1;
  ValueNode* cell = LoadFixedArrayElement(exports, cell_index);
  AddNewNode<StoreTaggedFieldWithWriteBarrier>({cell, GetAccumulatorTagged()},
                                               Cell::kValueOffset);
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

  if (TryBuildScriptContextAccess(global_access_feedback)) return;
  if (TryBuildPropertyCellAccess(global_access_feedback)) return;

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

    case compiler::ProcessedFeedback::kNamedAccess:
      if (TryBuildNamedAccess(object, object,
                              processed_feedback.AsNamedAccess(),
                              compiler::AccessMode::kStore)) {
        return;
      }
      break;
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

    case compiler::ProcessedFeedback::kNamedAccess:
      if (TryBuildNamedAccess(object, object,
                              processed_feedback.AsNamedAccess(),
                              compiler::AccessMode::kDefine)) {
        return;
      }
      break;

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
#define CASE(Name)                                                             \
  case Opcode::k##Name: {                                                      \
    SetAccumulator(                                                            \
        GetBooleanConstant(!value->Cast<Name>()->ToBoolean(local_isolate()))); \
    break;                                                                     \
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
#define CASE(Name)                                                             \
  case Opcode::k##Name: {                                                      \
    SetAccumulator(                                                            \
        GetBooleanConstant(!value->Cast<Name>()->ToBoolean(local_isolate()))); \
    break;                                                                     \
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

void MaglevGraphBuilder::VisitFindNonDefaultConstructorOrConstruct() {
  ValueNode* this_function = LoadRegisterTagged(0);
  ValueNode* new_target = LoadRegisterTagged(1);

  CallBuiltin* call_builtin =
      BuildCallBuiltin<Builtin::kFindNonDefaultConstructorOrConstruct>(
          {this_function, new_target});
  auto result = iterator_.GetRegisterPairOperand(2);
  StoreRegisterPair(result, call_builtin);
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
  BasicBlock* block = FinishBlock<JumpToInlined>({}, &start_ref, inner_unit);

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
  inner_graph_builder.FinishBlock<JumpFromInlined>({}, &end_ref);

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
  // Set the exit JumpFromInlined to jump to this resume block.
  // TODO(leszeks): Passing start_ref to JumpFromInlined creates a two-element
  // linked list of refs. Consider adding a helper to explicitly set the target
  // instead.
  end_ref.SetToBlockAndReturnNext(current_block_)
      ->SetToBlockAndReturnNext(current_block_);
}

bool MaglevGraphBuilder::TryReduceStringFromCharCode(
    compiler::JSFunctionRef target, const CallArguments& args) {
  if (args.count() != 1) return false;
  SetAccumulator(AddNewNode<BuiltinStringFromCharCode>({GetInt32(args[0])}));
  return true;
}

bool MaglevGraphBuilder::TryReduceStringPrototypeCharCodeAt(
    compiler::JSFunctionRef target, const CallArguments& args) {
  ValueNode* receiver = GetTaggedReceiver(args);
  ValueNode* index;
  if (args.count() == 0) {
    // Index is the undefined object. ToIntegerOrInfinity(undefined) = 0.
    index = GetInt32Constant(0);
  } else {
    index = GetInt32ElementIndex(args[0]);
  }
  // Any other argument is ignored.
  // Ensure that {receiver} is actually a String.
  BuildCheckString(receiver);
  // And index is below length.
  ValueNode* length = AddNewNode<StringLength>({receiver});
  AddNewNode<CheckInt32Condition>({index, length}, AssertCondition::kLess,
                                  DeoptimizeReason::kOutOfBounds);
  SetAccumulator(
      AddNewNode<BuiltinStringPrototypeCharCodeAt>({receiver, index}));
  return true;
}

bool MaglevGraphBuilder::TryReduceFunctionPrototypeCall(
    compiler::JSFunctionRef target, const CallArguments& args) {
  // Use Function.prototype.call context, to ensure any exception is thrown in
  // the correct context.
  ValueNode* context = GetConstant(target.context());
  ValueNode* receiver = GetTaggedReceiver(args);
  compiler::FeedbackSource feedback_source;
  BuildGenericCall(receiver, context, Call::TargetType::kAny,
                   args.PopReceiver(ConvertReceiverMode::kAny),
                   feedback_source);
  return true;
}

bool MaglevGraphBuilder::TryReduceBuiltin(compiler::JSFunctionRef target,
                                          const CallArguments& args) {
  if (!target.shared().HasBuiltinId()) return false;
  switch (target.shared().builtin_id()) {
#define CASE(Name)       \
  case Builtin::k##Name: \
    return TryReduce##Name(target, args);
    MAGLEV_REDUCED_BUILTIN(CASE)
#undef CASE
    default:
      // TODO(v8:7700): Inline more builtins.
      return false;
  }
}

ValueNode* MaglevGraphBuilder::GetConvertReceiver(
    compiler::JSFunctionRef function, const CallArguments& args) {
  compiler::SharedFunctionInfoRef shared = function.shared();
  if (shared.native() || shared.language_mode() == LanguageMode::kStrict) {
    if (args.receiver_mode() == ConvertReceiverMode::kNullOrUndefined) {
      return GetRootConstant(RootIndex::kUndefinedValue);
    } else {
      return GetTaggedValue(*args.receiver());
    }
  }
  if (args.receiver_mode() == ConvertReceiverMode::kNullOrUndefined) {
    return GetConstant(function.native_context().global_proxy_object());
  }
  ValueNode* receiver = GetTaggedValue(*args.receiver());
  if (CheckType(receiver, NodeType::kJSReceiver)) return receiver;
  if (Constant* constant = receiver->TryCast<Constant>()) {
    const Handle<HeapObject> object = constant->object().object();
    if (object->IsUndefined() || object->IsNull()) {
      return GetConstant(function.native_context().global_proxy_object());
    } else if (object->IsJSReceiver()) {
      return constant;
    }
  }
  return AddNewNode<ConvertReceiver>({receiver}, function,
                                     args.receiver_mode());
}

bool MaglevGraphBuilder::TryBuildCallKnownJSFunction(
    compiler::JSFunctionRef function, const CallArguments& args) {
  // Don't inline CallFunction stub across native contexts.
  if (function.native_context() != broker()->target_native_context()) {
    return false;
  }
  ValueNode* receiver = GetConvertReceiver(function, args);
  size_t input_count = args.count() + CallKnownJSFunction::kFixedInputCount;
  CallKnownJSFunction* call =
      CreateNewNode<CallKnownJSFunction>(input_count, function, receiver);
  for (int i = 0; i < args.count(); i++) {
    call->set_arg(i, GetTaggedValue(args[i]));
  }
  SetAccumulator(AddNode(call));
  return true;
}

void MaglevGraphBuilder::BuildGenericCall(
    ValueNode* target, ValueNode* context, Call::TargetType target_type,
    const CallArguments& args, compiler::FeedbackSource& feedback_source) {
  size_t input_count = args.count_with_receiver() + Call::kFixedInputCount;
  Call* call =
      CreateNewNode<Call>(input_count, args.receiver_mode(), target_type,
                          feedback_source, target, context);
  int arg_index = 0;
  call->set_arg(arg_index++, GetTaggedReceiver(args));
  for (int i = 0; i < args.count(); ++i) {
    call->set_arg(arg_index++, GetTaggedValue(args[i]));
  }
  SetAccumulator(AddNode(call));
}

bool MaglevGraphBuilder::BuildCheckValue(ValueNode* node,
                                         const compiler::HeapObjectRef& ref) {
  if (node->Is<Constant>()) {
    if (node->Cast<Constant>()->object().equals(ref)) return true;
    EmitUnconditionalDeopt(DeoptimizeReason::kUnknown);
    return false;
  }
  AddNewNode<CheckValue>({node}, ref);
  return true;
}

void MaglevGraphBuilder::BuildCall(ValueNode* target_node,
                                   const CallArguments& args,
                                   compiler::FeedbackSource& feedback_source) {
  Call::TargetType target_type = Call::TargetType::kAny;
  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForCall(feedback_source);
  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForCall);
      return;

    case compiler::ProcessedFeedback::kCall: {
      const compiler::CallFeedback& call_feedback = processed_feedback.AsCall();
      CallFeedbackContent content = call_feedback.call_feedback_content();
      if (content != CallFeedbackContent::kTarget) break;

      base::Optional<compiler::HeapObjectRef> maybe_target =
          call_feedback.target();
      if (!maybe_target.has_value()) break;

      compiler::HeapObjectRef target = maybe_target.value();
      if (!target.IsJSFunction()) break;
      compiler::JSFunctionRef function = target.AsJSFunction();

      // Do not reduce calls to functions with break points.
      if (function.shared().HasBreakInfo()) break;

      // Reset the feedback source
      feedback_source = compiler::FeedbackSource();
      target_type = Call::TargetType::kJSFunction;
      if (!BuildCheckValue(target_node, target)) return;

      if (function.object()->IsJSClassConstructor()) {
        // If we have a class constructor, we should raise an exception.
        SetAccumulator(BuildCallRuntime(
            Runtime::kThrowConstructorNonCallableError, {target_node}));
        return;
      }

      DCHECK(function.object()->IsCallable());
      if (TryReduceBuiltin(function, args)) {
        return;
      }
      if (TryBuildCallKnownJSFunction(function, args)) {
        return;
      }
      break;
    }

    default:
      break;
  }

  // On fallthrough, create a generic call.
  ValueNode* context = GetContext();
  BuildGenericCall(target_node, context, target_type, args, feedback_source);
}

void MaglevGraphBuilder::BuildCallFromRegisterList(
    ConvertReceiverMode receiver_mode) {
  ValueNode* target = LoadRegisterTagged(0);
  interpreter::RegisterList reg_list = iterator_.GetRegisterListOperand(1);
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source(feedback(), slot);
  CallArguments args(receiver_mode, reg_list);
  BuildCall(target, args, feedback_source);
}

void MaglevGraphBuilder::BuildCallFromRegisters(
    int arg_count, ConvertReceiverMode receiver_mode) {
  ValueNode* target = LoadRegisterTagged(0);
  const int receiver_count =
      (receiver_mode == ConvertReceiverMode::kNullOrUndefined) ? 0 : 1;
  const int reg_count = arg_count + receiver_count;
  int slot_operand_index = arg_count + receiver_count + 1;
  FeedbackSlot slot = GetSlotOperand(slot_operand_index);
  compiler::FeedbackSource feedback_source(feedback(), slot);
  switch (reg_count) {
    case 0: {
      CallArguments args(receiver_mode, reg_count);
      BuildCall(target, args, feedback_source);
      break;
    }
    case 1: {
      CallArguments args(receiver_mode, reg_count,
                         iterator_.GetRegisterOperand(1));
      BuildCall(target, args, feedback_source);
      break;
    }
    case 2: {
      CallArguments args(receiver_mode, reg_count,
                         iterator_.GetRegisterOperand(1),
                         iterator_.GetRegisterOperand(2));
      BuildCall(target, args, feedback_source);
      break;
    }
    case 3: {
      CallArguments args(
          receiver_mode, reg_count, iterator_.GetRegisterOperand(1),
          iterator_.GetRegisterOperand(2), iterator_.GetRegisterOperand(3));
      BuildCall(target, args, feedback_source);
      break;
    }
    default:
      UNREACHABLE();
  }
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
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source(feedback(), slot);

  size_t input_count = args.register_count() + CallWithSpread::kFixedInputCount;
  CallWithSpread* call = CreateNewNode<CallWithSpread>(
      input_count, feedback_source, function, context);
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
  ValueNode* callee = LoadAndCacheContextSlot(
      context, NativeContext::OffsetOfElementAt(slot), kMutable);
  // Call the function.
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  int kTheReceiver = 1;
  size_t input_count =
      args.register_count() + Call::kFixedInputCount + kTheReceiver;

  Call* call =
      CreateNewNode<Call>(input_count, ConvertReceiverMode::kNullOrUndefined,
                          Call::TargetType::kJSFunction,
                          compiler::FeedbackSource(), callee, GetContext());
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

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorYieldWithAwait(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 3);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorYieldWithAwait>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1]),
       GetTaggedValue(args[2])}));
}

void MaglevGraphBuilder::VisitConstruct() {
  ValueNode* new_target = GetAccumulatorTagged();
  ValueNode* constructor = LoadRegisterTagged(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  size_t input_count = args.register_count() + 1 + Construct::kFixedInputCount;
  Construct* construct = CreateNewNode<Construct>(
      input_count, feedback_source, constructor, new_target, context);
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
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source(feedback(), slot);

  int kReceiver = 1;
  size_t input_count =
      args.register_count() + kReceiver + ConstructWithSpread::kFixedInputCount;
  ConstructWithSpread* construct = CreateNewNode<ConstructWithSpread>(
      input_count, feedback_source, constructor, new_target, context);
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

MaglevGraphBuilder::InferHasInPrototypeChainResult
MaglevGraphBuilder::InferHasInPrototypeChain(
    ValueNode* receiver, compiler::HeapObjectRef prototype) {
  auto stable_it = known_node_aspects().stable_maps.find(receiver);
  auto unstable_it = known_node_aspects().unstable_maps.find(receiver);
  auto stable_end = known_node_aspects().stable_maps.end();
  auto unstable_end = known_node_aspects().unstable_maps.end();
  // If either of the map sets is not found, then we don't know anything about
  // the map of the receiver, so bail.
  if (stable_it == stable_end || unstable_it == unstable_end) {
    return kMayBeInPrototypeChain;
  }

  ZoneVector<compiler::MapRef> receiver_map_refs(zone());

  // Try to determine either that all of the {receiver_maps} have the given
  // {prototype} in their chain, or that none do. If we can't tell, return
  // kMayBeInPrototypeChain.
  bool all = true;
  bool none = true;
  for (const ZoneHandleSet<Map>& map_set :
       {stable_it->second, unstable_it->second}) {
    for (Handle<Map> map_handle : map_set) {
      compiler::MapRef map = MakeRefAssumeMemoryFence(broker(), map_handle);
      receiver_map_refs.push_back(map);
      while (true) {
        if (IsSpecialReceiverInstanceType(map.instance_type())) {
          return kMayBeInPrototypeChain;
        }
        if (!map.IsJSObjectMap()) {
          all = false;
          break;
        }
        compiler::HeapObjectRef map_prototype = map.prototype();
        if (map_prototype.equals(prototype)) {
          none = false;
          break;
        }
        map = map_prototype.map();
        // TODO(v8:11457) Support dictionary mode protoypes here.
        if (!map.is_stable() || map.is_dictionary_map()) {
          return kMayBeInPrototypeChain;
        }
        if (map.oddball_type() == compiler::OddballType::kNull) {
          all = false;
          break;
        }
      }
    }
  }
  DCHECK_IMPLIES(all, !none);
  if (!all && !none) return kMayBeInPrototypeChain;

  {
    base::Optional<compiler::JSObjectRef> last_prototype;
    if (all) {
      // We don't need to protect the full chain if we found the prototype, we
      // can stop at {prototype}.  In fact we could stop at the one before
      // {prototype} but since we're dealing with multiple receiver maps this
      // might be a different object each time, so it's much simpler to include
      // {prototype}. That does, however, mean that we must check {prototype}'s
      // map stability.
      if (!prototype.map().is_stable()) return kMayBeInPrototypeChain;
      last_prototype = prototype.AsJSObject();
    }
    broker()->dependencies()->DependOnStablePrototypeChains(
        receiver_map_refs, kStartAtPrototype, last_prototype);
  }

  DCHECK_EQ(all, !none);
  return all ? kIsInPrototypeChain : kIsNotInPrototypeChain;
}

bool MaglevGraphBuilder::TryBuildFastHasInPrototypeChain(
    ValueNode* object, compiler::ObjectRef prototype) {
  if (!prototype.IsHeapObject()) return false;
  auto in_prototype_chain =
      InferHasInPrototypeChain(object, prototype.AsHeapObject());
  if (in_prototype_chain == kMayBeInPrototypeChain) return false;

  SetAccumulator(GetBooleanConstant(in_prototype_chain == kIsInPrototypeChain));
  return true;
}

void MaglevGraphBuilder::BuildHasInPrototypeChain(
    ValueNode* object, compiler::ObjectRef prototype) {
  if (TryBuildFastHasInPrototypeChain(object, prototype)) return;

  SetAccumulator(BuildCallRuntime(Runtime::kHasInPrototypeChain,
                                  {object, GetConstant(prototype)}));
}

bool MaglevGraphBuilder::TryBuildFastOrdinaryHasInstance(
    ValueNode* object, compiler::JSObjectRef callable,
    ValueNode* callable_node_if_not_constant) {
  const bool is_constant = callable_node_if_not_constant == nullptr;
  if (!is_constant) return false;

  if (callable.IsJSBoundFunction()) {
    // OrdinaryHasInstance on bound functions turns into a recursive
    // invocation of the instanceof operator again.
    compiler::JSBoundFunctionRef function = callable.AsJSBoundFunction();
    compiler::JSReceiverRef bound_target_function =
        function.bound_target_function();

    if (!bound_target_function.IsJSObject() ||
        !TryBuildFastInstanceOf(object, bound_target_function.AsJSObject(),
                                nullptr)) {
      // If we can't build a fast instance-of, build a slow one with the
      // partial optimisation of using the bound target function constant.
      SetAccumulator(BuildCallBuiltin<Builtin::kInstanceOf>(
          {object, GetConstant(bound_target_function)}));
    }
    return true;
  }

  if (callable.IsJSFunction()) {
    // Optimize if we currently know the "prototype" property.
    compiler::JSFunctionRef function = callable.AsJSFunction();

    // TODO(v8:7700): Remove the has_prototype_slot condition once the broker
    // is always enabled.
    if (!function.map().has_prototype_slot() ||
        !function.has_instance_prototype(broker()->dependencies()) ||
        function.PrototypeRequiresRuntimeLookup(broker()->dependencies())) {
      return false;
    }

    compiler::ObjectRef prototype =
        broker()->dependencies()->DependOnPrototypeProperty(function);
    BuildHasInPrototypeChain(object, prototype);
    return true;
  }

  return false;
}

void MaglevGraphBuilder::BuildOrdinaryHasInstance(
    ValueNode* object, compiler::JSObjectRef callable,
    ValueNode* callable_node_if_not_constant) {
  if (TryBuildFastOrdinaryHasInstance(object, callable,
                                      callable_node_if_not_constant))
    return;

  SetAccumulator(BuildCallBuiltin<Builtin::kOrdinaryHasInstance>(
      {object, callable_node_if_not_constant ? callable_node_if_not_constant
                                             : GetConstant(callable)}));
}

bool MaglevGraphBuilder::TryBuildFastInstanceOf(
    ValueNode* object, compiler::JSObjectRef callable,
    ValueNode* callable_node_if_not_constant) {
  compiler::MapRef receiver_map = callable.map();
  compiler::NameRef name =
      MakeRef(broker(), local_isolate()->factory()->has_instance_symbol());
  compiler::PropertyAccessInfo access_info = broker()->GetPropertyAccessInfo(
      receiver_map, name, compiler::AccessMode::kLoad,
      broker()->dependencies());

  // TODO(v8:11457) Support dictionary mode holders here.
  if (access_info.IsInvalid() || access_info.HasDictionaryHolder()) {
    return false;
  }
  access_info.RecordDependencies(broker()->dependencies());

  if (access_info.IsNotFound()) {
    // If there's no @@hasInstance handler, the OrdinaryHasInstance operation
    // takes over, but that requires the constructor to be callable.
    if (!receiver_map.is_callable()) {
      return false;
    }

    broker()->dependencies()->DependOnStablePrototypeChains(
        access_info.lookup_start_object_maps(), kStartAtPrototype);

    // Monomorphic property access.
    if (callable_node_if_not_constant) {
      BuildCheckMaps(callable_node_if_not_constant,
                     access_info.lookup_start_object_maps());
    }

    BuildOrdinaryHasInstance(object, callable, callable_node_if_not_constant);
    return true;
  }

  if (access_info.IsFastDataConstant()) {
    base::Optional<compiler::JSObjectRef> holder = access_info.holder();
    bool found_on_proto = holder.has_value();
    compiler::JSObjectRef holder_ref =
        found_on_proto ? holder.value() : callable;
    base::Optional<compiler::ObjectRef> has_instance_field =
        holder_ref.GetOwnFastDataProperty(access_info.field_representation(),
                                          access_info.field_index(),
                                          broker()->dependencies());
    if (!has_instance_field.has_value() ||
        !has_instance_field->IsHeapObject() ||
        !has_instance_field->AsHeapObject().map().is_callable()) {
      return false;
    }

    if (found_on_proto) {
      broker()->dependencies()->DependOnStablePrototypeChains(
          access_info.lookup_start_object_maps(), kStartAtPrototype,
          holder.value());
    }

    ValueNode* callable_node;
    if (callable_node_if_not_constant) {
      // Check that {callable_node_if_not_constant} is actually {callable}.
      AddNewNode<CheckValue>({callable_node_if_not_constant}, callable);
      callable_node = callable_node_if_not_constant;
    } else {
      callable_node = GetConstant(callable);
    }

    // Call @@hasInstance
    Call* call = AddNewNode<Call>(
        Call::kFixedInputCount + 2, ConvertReceiverMode::kNotNullOrUndefined,
        Call::TargetType::kJSFunction, compiler::FeedbackSource(),
        GetConstant(*has_instance_field), GetContext());
    call->set_arg(0, callable_node);
    call->set_arg(1, object);

    // Make sure that a lazy deopt after the @@hasInstance call also performs
    // ToBoolean before returning to the interpreter.
    // TODO(leszeks): Wrap this in a helper.
    new (call->lazy_deopt_info()) LazyDeoptInfo(
        zone(),
        BuiltinContinuationDeoptFrame(
            Builtin::kToBooleanLazyDeoptContinuation, {}, GetContext(),
            zone()->New<InterpretedDeoptFrame>(
                call->lazy_deopt_info()->top_frame().as_interpreted())));

    SetAccumulator(AddNewNode<ToBoolean>({call}));
    return true;
  }

  return false;
}

bool MaglevGraphBuilder::TryBuildFastInstanceOfWithFeedback(
    ValueNode* object, ValueNode* callable,
    compiler::FeedbackSource feedback_source) {
  compiler::ProcessedFeedback const& feedback =
      broker()->GetFeedbackForInstanceOf(feedback_source);

  // TurboFan emits generic code when there's no feedback, rather than
  // deopting.
  if (feedback.IsInsufficient()) return false;

  // Check if the right hand side is a known receiver, or
  // we have feedback from the InstanceOfIC.
  if (callable->Is<Constant>() &&
      callable->Cast<Constant>()->object().IsJSObject()) {
    compiler::JSObjectRef callable_ref =
        callable->Cast<Constant>()->object().AsJSObject();
    return TryBuildFastInstanceOf(object, callable_ref, nullptr);
  }
  if (feedback_source.IsValid()) {
    base::Optional<compiler::JSObjectRef> callable_from_feedback =
        feedback.AsInstanceOf().value();
    if (callable_from_feedback) {
      return TryBuildFastInstanceOf(object, *callable_from_feedback, callable);
    }
  }
  return false;
}

void MaglevGraphBuilder::VisitTestInstanceOf() {
  // TestInstanceOf <src> <feedback_slot>
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* callable = GetAccumulatorTagged();
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  if (TryBuildFastInstanceOfWithFeedback(object, callable, feedback_source)) {
    return;
  }

  ValueNode* context = GetContext();
  SetAccumulator(
      AddNewNode<TestInstanceOf>({context, object, callable}, feedback_source));
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
  if (CheckType(value, NodeType::kName)) {
    MoveNodeBetweenRegisters(interpreter::Register::virtual_accumulator(),
                             destination);
  } else {
    StoreRegister(destination, AddNewNode<ToName>({GetContext(), value}));
  }
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
    case BinaryOperationHint::kBigInt64:
      if (mode == Object::Conversion::kToNumber &&
          EnsureType(value, NodeType::kNumber)) {
        return;
      }
      AddNewNode<CheckNumber>({value}, mode);
      break;
    default:
      if (CheckType(value, NodeType::kNumber)) return;
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
  if (CheckType(value, NodeType::kJSReceiver)) {
    MoveNodeBetweenRegisters(interpreter::Register::virtual_accumulator(),
                             destination);
  } else {
    StoreRegister(destination, AddNewNode<ToObject>({GetContext(), value}));
  }
}

void MaglevGraphBuilder::VisitToString() {
  // ToString
  ValueNode* value = GetAccumulatorTagged();
  if (CheckType(value, NodeType::kString)) return;
  // TODO(victorgomes): Add fast path for constant primitives.
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
  // TODO(v8:7700): Update TryGetParentContext if this ever emits its own Node
  // type.
  // CreateBlockContext <scope_info_idx>
  ValueNode* scope_info = GetConstant(GetRefOperand<ScopeInfo>(0));
  SetAccumulator(BuildCallRuntime(Runtime::kPushBlockContext, {scope_info}));
}

void MaglevGraphBuilder::VisitCreateCatchContext() {
  // TODO(v8:7700): Inline allocation when context is small.
  // TODO(v8:7700): Update TryGetParentContext if this ever emits its own Node
  // type.
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
  // TODO(v8:7700): Update TryGetParentContext if this ever emits its own Node
  // type.
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

  if (!is_toptier()) {
    if (relative_jump_bytecode_offset > 0) {
      AddNewNode<ReduceInterruptBudget>({}, relative_jump_bytecode_offset);
    }
    AddNewNode<JumpLoopPrologue>({}, loop_offset, feedback_slot,
                                 BytecodeOffset(iterator_.current_offset()),
                                 compilation_unit_);
  }
  BasicBlock* block =
      FinishBlock<JumpLoop>({}, jump_targets_[target].block_ptr());

  merge_states_[target]->MergeLoop(*compilation_unit_, graph_->smi(),
                                   current_interpreter_frame_, block, target);
  block->set_predecessor_id(merge_states_[target]->predecessor_count() - 1);
}
void MaglevGraphBuilder::VisitJump() {
  const uint32_t relative_jump_bytecode_offset =
      iterator_.GetRelativeJumpTargetOffset();
  if (!is_toptier() && relative_jump_bytecode_offset > 0) {
    AddNewNode<IncreaseInterruptBudget>({}, relative_jump_bytecode_offset);
  }
  BasicBlock* block =
      FinishBlock<Jump>({}, &jump_targets_[iterator_.GetJumpTargetOffset()]);
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
    merge_states_[target]->Merge(*compilation_unit_, graph_->smi(),
                                 current_interpreter_frame_, predecessor,
                                 target);
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
      if (v8_flags.trace_maglev_graph_building) {
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
    merge_states_[target]->Merge(*compilation_unit_, graph_->smi(),
                                 current_interpreter_frame_, predecessor,
                                 target);
  }
}

void MaglevGraphBuilder::BuildBranchIfRootConstant(ValueNode* node,
                                                   JumpType jump_type,
                                                   RootIndex root_index) {
  int fallthrough_offset = next_offset();
  int jump_offset = iterator_.GetJumpTargetOffset();
  if (RootConstant* c = node->TryCast<RootConstant>()) {
    bool constant_is_match = c->index() == root_index;
    bool is_jump_taken = constant_is_match == (jump_type == kJumpIfTrue);
    if (is_jump_taken) {
      BasicBlock* block = FinishBlock<Jump>({}, &jump_targets_[jump_offset]);
      MergeDeadIntoFrameState(fallthrough_offset);
      MergeIntoFrameState(block, jump_offset);
    } else {
      MergeDeadIntoFrameState(jump_offset);
    }
    return;
  }

  BasicBlockRef* true_target = jump_type == kJumpIfTrue
                                   ? &jump_targets_[jump_offset]
                                   : &jump_targets_[fallthrough_offset];
  BasicBlockRef* false_target = jump_type == kJumpIfFalse
                                    ? &jump_targets_[jump_offset]
                                    : &jump_targets_[fallthrough_offset];

  BasicBlock* block = FinishBlock<BranchIfRootConstant>(
      {node}, true_target, false_target, root_index);
  if (jump_type == kJumpIfTrue) {
    block->control_node()
        ->Cast<BranchControlNode>()
        ->set_true_interrupt_correction(
            iterator_.GetRelativeJumpTargetOffset());
  } else {
    block->control_node()
        ->Cast<BranchControlNode>()
        ->set_false_interrupt_correction(
            iterator_.GetRelativeJumpTargetOffset());
  }
  MergeIntoFrameState(block, jump_offset);
  StartFallthroughBlock(fallthrough_offset, block);
}
void MaglevGraphBuilder::BuildBranchIfTrue(ValueNode* node,
                                           JumpType jump_type) {
  BuildBranchIfRootConstant(node, jump_type, RootIndex::kTrueValue);
}
void MaglevGraphBuilder::BuildBranchIfNull(ValueNode* node,
                                           JumpType jump_type) {
  BuildBranchIfRootConstant(node, jump_type, RootIndex::kNullValue);
}
void MaglevGraphBuilder::BuildBranchIfUndefined(ValueNode* node,
                                                JumpType jump_type) {
  BuildBranchIfRootConstant(node, jump_type, RootIndex::kUndefinedValue);
}
void MaglevGraphBuilder::BuildBranchIfToBooleanTrue(ValueNode* node,
                                                    JumpType jump_type) {
  int fallthrough_offset = next_offset();
  int jump_offset = iterator_.GetJumpTargetOffset();
  BasicBlockRef* true_target = jump_type == kJumpIfTrue
                                   ? &jump_targets_[jump_offset]
                                   : &jump_targets_[fallthrough_offset];
  BasicBlockRef* false_target = jump_type == kJumpIfFalse
                                    ? &jump_targets_[jump_offset]
                                    : &jump_targets_[fallthrough_offset];
  BasicBlock* block =
      FinishBlock<BranchIfToBooleanTrue>({node}, true_target, false_target);
  if (jump_type == kJumpIfTrue) {
    block->control_node()
        ->Cast<BranchControlNode>()
        ->set_true_interrupt_correction(
            iterator_.GetRelativeJumpTargetOffset());
  } else {
    block->control_node()
        ->Cast<BranchControlNode>()
        ->set_false_interrupt_correction(
            iterator_.GetRelativeJumpTargetOffset());
  }
  MergeIntoFrameState(block, jump_offset);
  StartFallthroughBlock(fallthrough_offset, block);
}
void MaglevGraphBuilder::VisitJumpIfToBooleanTrue() {
  BuildBranchIfToBooleanTrue(GetAccumulatorTagged(), kJumpIfTrue);
}
void MaglevGraphBuilder::VisitJumpIfToBooleanFalse() {
  BuildBranchIfToBooleanTrue(GetAccumulatorTagged(), kJumpIfFalse);
}
void MaglevGraphBuilder::VisitJumpIfTrue() {
  BuildBranchIfTrue(GetAccumulatorTagged(), kJumpIfTrue);
}
void MaglevGraphBuilder::VisitJumpIfFalse() {
  BuildBranchIfTrue(GetAccumulatorTagged(), kJumpIfFalse);
}
void MaglevGraphBuilder::VisitJumpIfNull() {
  BuildBranchIfNull(GetAccumulatorTagged(), kJumpIfTrue);
}
void MaglevGraphBuilder::VisitJumpIfNotNull() {
  BuildBranchIfNull(GetAccumulatorTagged(), kJumpIfFalse);
}
void MaglevGraphBuilder::VisitJumpIfUndefined() {
  BuildBranchIfUndefined(GetAccumulatorTagged(), kJumpIfTrue);
}
void MaglevGraphBuilder::VisitJumpIfNotUndefined() {
  BuildBranchIfUndefined(GetAccumulatorTagged(), kJumpIfFalse);
}
void MaglevGraphBuilder::VisitJumpIfUndefinedOrNull() {
  BasicBlock* block = FinishBlock<BranchIfUndefinedOrNull>(
      {GetAccumulatorTagged()}, &jump_targets_[iterator_.GetJumpTargetOffset()],
      &jump_targets_[next_offset()]);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
  StartFallthroughBlock(next_offset(), block);
}
void MaglevGraphBuilder::VisitJumpIfJSReceiver() {
  BasicBlock* block = FinishBlock<BranchIfJSReceiver>(
      {GetAccumulatorTagged()}, &jump_targets_[iterator_.GetJumpTargetOffset()],
      &jump_targets_[next_offset()]);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
  StartFallthroughBlock(next_offset(), block);
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
      FinishBlock<Switch>({case_value}, case_value_base, targets,
                          offsets.size(), &jump_targets_[next_offset()]);
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    MergeIntoFrameState(block, offset.target_offset);
  }
  StartFallthroughBlock(next_offset(), block);
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
  // TODO(verwaest): Fold with the next instruction.
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
  if (!is_toptier() && relative_jump_bytecode_offset > 0) {
    AddNewNode<ReduceInterruptBudget>({}, relative_jump_bytecode_offset);
  }

  if (!is_inline()) {
    FinishBlock<Return>({GetAccumulatorTagged()});
    return;
  }

  // All inlined function returns instead jump to one past the end of the
  // bytecode, where we'll later create a final basic block which resumes
  // execution of the caller.
  // TODO(leszeks): Consider shortcutting this Jump for cases where there is
  // only one return and no need to merge return states.
  BasicBlock* block =
      FinishBlock<Jump>({}, &jump_targets_[inline_exit_offset()]);
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
  DCHECK_EQ(iterator_.current_offset(), 0);
  int generator_prologue_block_offset = 1;
  DCHECK_LT(generator_prologue_block_offset, next_offset());

  interpreter::JumpTableTargetOffsets offsets =
      iterator_.GetJumpTableTargetOffsets();
  // If there are no jump offsets, then this generator is not resumable, which
  // means we can skip checking for it and switching on its state.
  if (offsets.size() == 0) return;

  // We create an initial block that checks if the generator is undefined.
  ValueNode* maybe_generator = LoadRegisterTagged(0);
  BasicBlock* block_is_generator_undefined = FinishBlock<BranchIfRootConstant>(
      {maybe_generator}, &jump_targets_[next_offset()],
      &jump_targets_[generator_prologue_block_offset],
      RootIndex::kUndefinedValue);
  MergeIntoFrameState(block_is_generator_undefined, next_offset());

  // We create the generator prologue block.
  StartNewBlock(generator_prologue_block_offset);

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
  int case_value_base = (*offsets.begin()).case_value;
  BasicBlockRef* targets = zone()->NewArray<BasicBlockRef>(offsets.size());
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    BasicBlockRef* ref = &targets[offset.case_value - case_value_base];
    new (ref) BasicBlockRef(&jump_targets_[offset.target_offset]);
  }
  ValueNode* case_value = AddNewNode<UnsafeSmiUntag>({state});
  BasicBlock* generator_prologue_block = FinishBlock<Switch>(
      {case_value}, case_value_base, targets, offsets.size());
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    MergeIntoFrameState(generator_prologue_block, offset.target_offset);
  }
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
  if (!is_toptier() && relative_jump_bytecode_offset > 0) {
    AddNewNode<ReduceInterruptBudget>({}, relative_jump_bytecode_offset);
  }
  FinishBlock<Return>({GetAccumulatorTagged()});
}

void MaglevGraphBuilder::VisitResumeGenerator() {
  // ResumeGenerator <generator> <first output register> <register count>
  ValueNode* generator = LoadRegisterTagged(0);
  ValueNode* array = AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kParametersAndRegistersOffset);
  interpreter::RegisterList registers = iterator_.GetRegisterListOperand(1);

  if (v8_flags.maglev_assert) {
    // Check if register count is invalid, that is, larger than the
    // register file length.
    ValueNode* array_length_smi =
        AddNewNode<LoadTaggedField>({array}, FixedArrayBase::kLengthOffset);
    ValueNode* array_length = AddNewNode<UnsafeSmiUntag>({array_length_smi});
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

}  // namespace v8::internal::maglev
