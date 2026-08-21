// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-optimizer.h"

#include <optional>

#include "src/base/logging.h"
#include "src/common/operation.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-reducer-inl.h"
#include "src/maglev/maglev-reducer.h"

namespace v8 {
namespace internal {
namespace maglev {

#define RETURN_IF_SUCCESS(res) \
  do {                         \
    auto _res = (res);         \
    if (_res) return *_res;    \
  } while (false)

namespace {
constexpr ValueRepresentation ValueRepresentationFromUse(
    UseRepresentation repr) {
  switch (repr) {
    case UseRepresentation::kTagged:
      return ValueRepresentation::kTagged;
    case UseRepresentation::kInt32:
    case UseRepresentation::kTruncatedInt32:
      return ValueRepresentation::kInt32;
    case UseRepresentation::kUint32:
      return ValueRepresentation::kUint32;
    case UseRepresentation::kFloat64:
      return ValueRepresentation::kFloat64;
    case UseRepresentation::kHoleyFloat64:
      return ValueRepresentation::kHoleyFloat64;
  }
  UNREACHABLE();
}
}  // namespace

MaglevGraphOptimizer::MaglevGraphOptimizer(
    Graph* graph, RecomputeKnownNodeAspectsProcessor& kna_processor)
    : reducer_(this, graph), kna_processor_(kna_processor) {}

BlockProcessResult MaglevGraphOptimizer::PreProcessBasicBlock(
    BasicBlock* block) {
  reducer_.set_current_block(block);
  return BlockProcessResult::kContinue;
}

void MaglevGraphOptimizer::PostProcessBasicBlock(BasicBlock* block) {
  reducer_.FlushNodesToBlock();
}

void MaglevGraphOptimizer::PreProcessNode(Node*, const ProcessingState& state) {
#ifdef DEBUG
  reducer_.StartNewPeriod();
#endif  // DEBUG
  if (reducer_.has_graph_labeller()) {
    reducer_.SetCurrentProvenance(
        reducer_.graph_labeller()->GetNodeProvenance(current_node()));
  }
  reducer_.SetNewNodePosition(BasicBlockPosition::At(state.node_index()));
}

void MaglevGraphOptimizer::PostProcessNode(Node*) {
#ifdef DEBUG
  reducer_.SetCurrentProvenance(MaglevGraphLabeller::Provenance{});
  reducer_.SetNewNodePosition(BasicBlockPosition::End());
#endif  // DEBUG
}

void MaglevGraphOptimizer::PreProcessNode(Phi*, const ProcessingState&) {}
void MaglevGraphOptimizer::PostProcessNode(Phi*) {}

void MaglevGraphOptimizer::PreProcessNode(ControlNode*,
                                          const ProcessingState&) {
  reducer_.SetNewNodePosition(BasicBlockPosition::End());
}
void MaglevGraphOptimizer::PostProcessNode(ControlNode*) {}

compiler::JSHeapBroker* MaglevGraphOptimizer::broker() const {
  return reducer_.broker();
}

ValueNode* MaglevGraphOptimizer::GetInputAt(int index) const {
  CHECK_NOT_NULL(current_node_);
  DCHECK_LT(index, current_node()->input_count());
  ValueNode* input = current_node()->input(index).node();
  return input;
}

ProcessResult MaglevGraphOptimizer::ReplaceWith(ValueNode* node) {
  // If current node is not a value node, we shouldn't try to replace it.
  CHECK(current_node()->Cast<ValueNode>());
  DCHECK(!node->Is<Identity>());
  ValueNode* current_value = current_node()->Cast<ValueNode>();
  // Automatically convert node to the same representation of current_node.
  current_value->OverwriteWithIdentityTo(reducer_.ConvertInputTo(
      node, current_value->properties().value_representation()));
  return ProcessResult::kRemove;
}

void MaglevGraphOptimizer::UnwrapInputs() {
  for (int i = 0; i < current_node()->input_count(); i++) {
    ValueNode* input = current_node()->input(i).node();
    if (!input) continue;
    current_node()->change_input(i, input->UnwrapIdentities());
  }
}

ValueNode* MaglevGraphOptimizer::GetConstantWithRepresentation(
    ValueNode* node, UseRepresentation use_repr) {
  switch (use_repr) {
    case UseRepresentation::kInt32:
    case UseRepresentation::kTruncatedInt32: {
      auto cst = reducer_.TryGetInt32Constant(node);
      if (cst.has_value()) {
        return reducer_.GetInt32Constant(cst.value());
      }
      return nullptr;
    }
    case UseRepresentation::kFloat64:
    case UseRepresentation::kHoleyFloat64: {
      auto cst = reducer_.TryGetFloat64Constant(
          node, TaggedToFloat64ConversionType::kNumberOrOddball);
      if (cst.has_value()) {
        return reducer_.GetFloat64Constant(cst.value());
      }
      return nullptr;
    }
    default:
      return nullptr;
  }
}

ValueNode* MaglevGraphOptimizer::GetUntaggedValueWithRepresentation(
    ValueNode* node, UseRepresentation use_repr, NodeType allowed_type) {
  DCHECK_NE(use_repr, UseRepresentation::kTagged);
  if (node->value_representation() == ValueRepresentationFromUse(use_repr))
    return node;
  if (node->Is<ReturnedValue>()) {
    ValueNode* input = node->input_node(0);
    return GetUntaggedValueWithRepresentation(input, use_repr, allowed_type);
  }
  // We try getting constant before bailing out and/or calling the reducer,
  // since it does not emit a conversion node.
  if (auto cst = GetConstantWithRepresentation(node, use_repr)) return cst;
  if (node->is_tagged()) return nullptr;
  // TODO(victorgomes): The GetXXX functions may emit a conversion node that
  // might eager deopt. We need to find a correct eager deopt frame for them if
  // current_node_ does not have a deopt info.
  if (!current_node_->properties().can_eager_deopt() &&
      !current_node_->properties().is_deopt_checkpoint()) {
    return nullptr;
  }
  switch (use_repr) {
    case UseRepresentation::kInt32:
      return reducer_.GetInt32(node);
    case UseRepresentation::kTruncatedInt32:
      return reducer_.GetTruncatedInt32ForToNumber(node, allowed_type);
    case UseRepresentation::kFloat64:
      return reducer_.GetFloat64ForToNumber(node, allowed_type);
    case UseRepresentation::kHoleyFloat64:
      return reducer_.GetHoleyFloat64ForToNumber(node, allowed_type);
    default:
      return nullptr;
  }
  UNREACHABLE();
}

template <Operation kOperation>
std::optional<ProcessResult> MaglevGraphOptimizer::TryFoldInt32Operation() {
  MaybeReduceResult result;
  if constexpr (IsUnaryOperation(kOperation)) {
    result = reducer_.TryFoldInt32UnaryOperation<kOperation>(GetInputAt(0));
  } else {
    static_assert(IsBinaryOperation(kOperation));
    result = reducer_.TryFoldInt32BinaryOperation<kOperation>(GetInputAt(0),
                                                              GetInputAt(1));
  }
  if (!result.IsDone()) return {};
  DCHECK(result.IsDoneWithValue());
  if constexpr (kOperation == Operation::kShiftRightLogical) {
    // ShiftRightLogical returns an Uint32 instead of an Int32. We don't have
    // any peephole optimizations for ShiftRightLogical, if we managed to fold
    // it, it must have been because both inputs were constants and we got a
    // Uint32Constant.
    CHECK(result.value()->Is<Uint32Constant>());
    return ReplaceWith(result.value());
  }
  return ReplaceWith(reducer_.GetInt32(result.value()));
}

template <Operation kOperation>
std::optional<ProcessResult> MaglevGraphOptimizer::TryFoldFloat64Operation() {
  MaybeReduceResult result;
  if constexpr (IsUnaryOperation(kOperation)) {
    result = reducer_.TryFoldFloat64UnaryOperationForToNumber<kOperation>(
        TaggedToFloat64ConversionType::kOnlyNumber, GetInputAt(0));
  } else {
    static_assert(IsBinaryOperation(kOperation));
    result = reducer_.TryFoldFloat64BinaryOperationForToNumber<kOperation>(
        TaggedToFloat64ConversionType::kOnlyNumber, GetInputAt(0),
        GetInputAt(1));
  }
  if (!result.IsDone()) return {};
  DCHECK(result.IsDoneWithValue());
  return ReplaceWith(reducer_.GetFloat64(result.value()));
}

Jump* MaglevGraphOptimizer::FoldBranch(BasicBlock* current,
                                       BranchControlNode* branch_node,
                                       bool if_true) {
  BasicBlock* target =
      if_true ? branch_node->if_true() : branch_node->if_false();
  BasicBlock* unreachable_block =
      if_true ? branch_node->if_false() : branch_node->if_true();

  // Remove predecessor from unreachable block.
  if (!unreachable_block->has_state()) {
    unreachable_block->set_predecessor(nullptr);
  } else {
    for (int i = unreachable_block->predecessor_count() - 1; i >= 0; i--) {
      if (unreachable_block->predecessor_at(i) == current) {
        unreachable_block->state()->RemovePredecessorAt(i);
      }
    }
  }

  // Update control node.
  Jump* new_control_node = branch_node->OverwriteWith<Jump>();
  new_control_node->set_target(target);

  // Cache predecessor id from the target in the unconditional jump.
  int predecessor_id;
  if (!target->has_state()) {
    predecessor_id = 0;
  } else {
    for (int i = target->predecessor_count() - 1; i >= 0; i--) {
      if (target->predecessor_at(i) == current) {
        predecessor_id = i;
      }
    }
  }
  new_control_node->set_predecessor_id(predecessor_id);

  reducer_.graph()->set_may_have_unreachable_blocks(true);
  return new_control_node;
}

ReduceResult MaglevGraphOptimizer::EmitUnconditionalDeopt(
    DeoptimizeReason reason) {
  reducer_.current_block()->set_deferred(true);
  reducer_.current_block()->reset_control_node();
  reducer_.AddNewControlNode<Deopt>({}, reason);
  return ReduceResult::DoneWithAbort();
}

ProcessResult MaglevGraphOptimizer::VisitAssertInt32(
    AssertInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckDynamicValue(
    CheckDynamicValue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckInt32IsSmi(
    CheckInt32IsSmi* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckUint32IsSmi(
    CheckUint32IsSmi* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckIntPtrIsSmi(
    CheckIntPtrIsSmi* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckHoleyFloat64IsSmi(
    CheckHoleyFloat64IsSmi* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckHeapObject(
    CheckHeapObject* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckInt32Condition(
    CheckInt32Condition* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckCacheIndicesNotCleared(
    CheckCacheIndicesNotCleared* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckJSDataViewBounds(
    CheckJSDataViewBounds* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckTypedArrayBounds(
    CheckTypedArrayBounds* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckTypedArrayNotDetached(
    CheckTypedArrayNotDetached* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMaps(
    CheckMaps* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  MaybeReduceResult result =
      reducer_.TryFoldCheckMaps(GetInputAt(0), node->maps());
  if (result.IsDoneWithAbort()) {
    reducer_.graph()->set_may_have_unreachable_blocks(true);
    return ProcessResult::kTruncateBlock;
  }
  if (result.IsDone()) {
    return ProcessResult::kRemove;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMapsWithMigrationAndDeopt(
    CheckMapsWithMigrationAndDeopt* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMapsWithMigration(
    CheckMapsWithMigration* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMapsWithAlreadyLoadedMap(
    CheckMapsWithAlreadyLoadedMap* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckDetectableCallable(
    CheckDetectableCallable* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckJSReceiverOrNullOrUndefined(
    CheckJSReceiverOrNullOrUndefined* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckNotHole(
    CheckNotHole* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckHoleyFloat64NotHoleOrUndefined(
    CheckHoleyFloat64NotHoleOrUndefined* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}
#define VISIT_CHECK(Type)                                                 \
  ProcessResult MaglevGraphOptimizer::VisitCheck##Type(                   \
      Check##Type* node, const ProcessingState& state) {                  \
    if (NodeTypeIs(reducer_.GetType(GetInputAt(0)), NodeType::k##Type)) { \
      return ProcessResult::kRemove;                                      \
    }                                                                     \
    return ProcessResult::kContinue;                                      \
  }
VISIT_CHECK(Smi)
VISIT_CHECK(Number)
VISIT_CHECK(String)
VISIT_CHECK(SeqOneByteString)
VISIT_CHECK(StringOrStringWrapper)
VISIT_CHECK(StringOrOddball)
VISIT_CHECK(Symbol)
#undef PROCESS_CHECK

ProcessResult MaglevGraphOptimizer::VisitCheckValue(
    CheckValue* node, const ProcessingState& state) {
  ValueNode* input = GetInputAt(0);
  if (Constant* constant = input->TryCast<Constant>()) {
    if (constant->object() == node->value()) {
      return ProcessResult::kRemove;
    }
    // TODO(victorgomes): Support soft deopting and killing the rest of the
    // block.
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckValueEqualsInt32(
    CheckValueEqualsInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckFloat64SameValue(
    CheckFloat64SameValue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckValueEqualsString(
    CheckValueEqualsString* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckInstanceType(
    CheckInstanceType* node, const ProcessingState& state) {
  ValueNode* input = GetInputAt(0);
  if (input->Is<FastCreateClosure>()) {
    if (node->first_instance_type() == FIRST_JS_FUNCTION_TYPE &&
        node->last_instance_type() == LAST_JS_FUNCTION_TYPE) {
      // Don't need to emit this check, we know it is a closure.
      return ProcessResult::kRemove;
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDead(Dead* node,
                                              const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDebugBreak(
    DebugBreak* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFunctionEntryStackCheck(
    FunctionEntryStackCheck* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGeneratorStore(
    GeneratorStore* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTryOnStackReplacement(
    TryOnStackReplacement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreMap(
    StoreMap* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFixedArrayElementWithWriteBarrier(
    StoreFixedArrayElementWithWriteBarrier* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFixedArrayElementNoWriteBarrier(
    StoreFixedArrayElementNoWriteBarrier* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFixedDoubleArrayElement(
    StoreFixedDoubleArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreInt32(
    StoreInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFloat64(
    StoreFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreIntTypedArrayElement(
    StoreIntTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreDoubleTypedArrayElement(
    StoreDoubleTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreIntConstantTypedArrayElement(
    StoreIntConstantTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreDoubleConstantTypedArrayElement(
    StoreDoubleConstantTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreSignedIntDataViewElement(
    StoreSignedIntDataViewElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreDoubleDataViewElement(
    StoreDoubleDataViewElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreTaggedFieldNoWriteBarrier(
    StoreTaggedFieldNoWriteBarrier* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreTaggedFieldWithWriteBarrier(
    StoreTaggedFieldWithWriteBarrier* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreContextSlotWithWriteBarrier(
    StoreContextSlotWithWriteBarrier* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitStoreTrustedPointerFieldWithWriteBarrier(
    StoreTrustedPointerFieldWithWriteBarrier* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHandleNoHeapWritesInterrupt(
    HandleNoHeapWritesInterrupt* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReduceInterruptBudgetForLoop(
    ReduceInterruptBudgetForLoop* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReduceInterruptBudgetForReturn(
    ReduceInterruptBudgetForReturn* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowReferenceErrorIfHole(
    ThrowReferenceErrorIfHole* node, const ProcessingState& state) {
  // Avoid the check if we know it is not the hole.
  ValueNode* value = node->value().node();
  if (IsConstantNode(value->opcode())) {
    if (value->IsTheHoleValue()) {
      ValueNode* input = reducer_.GetConstant(node->name());

      node->OverwriteWith<Throw>();
      node->Cast<Throw>()->UpdateBitfield(
          Throw::kThrowAccessedUninitializedVariable,
          /*has_input*/ true);
      node->change_input(0, input);

      // TODO(dmercadier): insert an Abort and cut the current basic block.
      return ProcessResult::kContinue;
    }

    // Not the hole; removing.
    return ProcessResult::kRemove;
  }

  // TODO(b/424157317): Optimize further.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowSuperNotCalledIfHole(
    ThrowSuperNotCalledIfHole* node, const ProcessingState& state) {
  // Avoid the check if we know it is not the hole.
  ValueNode* value = node->value().node();
  if (IsConstantNode(value->opcode())) {
    if (value->IsTheHoleValue()) {
      node->OverwriteWith<Throw>();
      node->Cast<Throw>()->UpdateBitfield(Throw::kThrowSuperNotCalled,
                                          /*has_input*/ false);
      node->change_input(0, reducer_.GetSmiConstant(0));

      // TODO(dmercadier): insert an Abort and cut the current basic block.
      return ProcessResult::kContinue;
    }

    // Not the hole; removing.
    return ProcessResult::kRemove;
  }

  // TODO(b/424157317): Optimize further.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowSuperAlreadyCalledIfNotHole(
    ThrowSuperAlreadyCalledIfNotHole* node, const ProcessingState& state) {
  // Avoid the check if we know it is not the hole.
  ValueNode* value = node->value().node();
  if (IsConstantNode(value->opcode())) {
    if (!value->IsTheHoleValue()) {
      node->OverwriteWith<Throw>();
      node->Cast<Throw>()->UpdateBitfield(Throw::kThrowSuperAlreadyCalledError,
                                          /*has_input*/ false);
      node->change_input(0, reducer_.GetSmiConstant(0));

      // TODO(dmercadier): insert an Abort and cut the current basic block.
      return ProcessResult::kContinue;
    }

    // Value is the hole; removing.
    return ProcessResult::kRemove;
  }

  // TODO(b/424157317): Optimize further.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowIfNotCallable(
    ThrowIfNotCallable* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowIfNotSuperConstructor(
    ThrowIfNotSuperConstructor* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTransitionElementsKindOrCheckMap(
    TransitionElementsKindOrCheckMap* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetContinuationPreservedEmbedderData(
    SetContinuationPreservedEmbedderData* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTransitionAndStoreArrayElement(
    TransitionAndStoreArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConstantGapMove(
    ConstantGapMove* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGapMove(GapMove* node,
                                                 const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitIdentity(
    Identity* node, const ProcessingState& state) {
  // If a non-eager inlined function returns a tagged value, we substitute the
  // call with an Identity. The node is then removed from the graph here. All
  // references to it will be removed in this graph optimizer pass.
  return ProcessResult::kRemove;
}

ProcessResult MaglevGraphOptimizer::VisitAllocationBlock(
    AllocationBlock* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitArgumentsElements(
    ArgumentsElements* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitArgumentsLength(
    ArgumentsLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitRestLength(
    RestLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCall(Call* node,
                                              const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallBuiltin(
    CallBuiltin* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallCPPBuiltin(
    CallCPPBuiltin* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallForwardVarargs(
    CallForwardVarargs* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallRuntime(
    CallRuntime* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallWithArrayLike(
    CallWithArrayLike* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallWithSpread(
    CallWithSpread* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallKnownApiFunction(
    CallKnownApiFunction* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallKnownJSFunction(
    CallKnownJSFunction* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReturnedValue(
    ReturnedValue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallSelf(
    CallSelf* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConstruct(
    Construct* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckConstructResult(
    CheckConstructResult* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckDerivedConstructResult(
    CheckDerivedConstructResult* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConstructWithSpread(
    ConstructWithSpread* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConvertReceiver(
    ConvertReceiver* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConvertHoleToUndefined(
    ConvertHoleToUndefined* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateArrayLiteral(
    CreateArrayLiteral* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateShallowArrayLiteral(
    CreateShallowArrayLiteral* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateObjectLiteral(
    CreateObjectLiteral* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateShallowObjectLiteral(
    CreateShallowObjectLiteral* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateFunctionContext(
    CreateFunctionContext* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateClosure(
    CreateClosure* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFastCreateClosure(
    FastCreateClosure* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateRegExpLiteral(
    CreateRegExpLiteral* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDeleteProperty(
    DeleteProperty* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitEnsureWritableFastElements(
    EnsureWritableFastElements* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitExtendPropertiesBackingStore(
    ExtendPropertiesBackingStore* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInlinedAllocation(
    InlinedAllocation* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitForInPrepare(
    ForInPrepare* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitForInNext(
    ForInNext* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGeneratorRestoreRegister(
    GeneratorRestoreRegister* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetIterator(
    GetIterator* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetSecondReturnedValue(
    GetSecondReturnedValue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetTemplateObject(
    GetTemplateObject* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHasInPrototypeChain(
    HasInPrototypeChain* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInitialValue(
    InitialValue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedField(
    LoadTaggedField* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  if (node->offset() == HeapObject::kMapOffset) {
    if (auto constant = reducer_.TryGetConstant(node)) {
      compiler::MapRef map = constant->map(broker());
      if (map.is_stable()) {
        broker()->dependencies()->DependOnStableMap(map);
        return ReplaceWith(reducer_.GetConstant(map));
      }
    }
  }
  if (node->offset() == JSFunction::kFeedbackCellOffset) {
    if (auto input = GetInputAt(0)->TryCast<FastCreateClosure>()) {
      return ReplaceWith(reducer_.GetConstant(input->feedback_cell()));
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedFieldForProperty(
    LoadTaggedFieldForProperty* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedFieldForContextSlotNoCells(
    LoadTaggedFieldForContextSlotNoCells* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedFieldForContextSlot(
    LoadTaggedFieldForContextSlot* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadFloat64(
    LoadFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadInt32(
    LoadInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedFieldByFieldIndex(
    LoadTaggedFieldByFieldIndex* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadFixedArrayElement(
    LoadFixedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadFixedDoubleArrayElement(
    LoadFixedDoubleArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadHoleyFixedDoubleArrayElement(
    LoadHoleyFixedDoubleArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitLoadHoleyFixedDoubleArrayElementCheckedNotHole(
    LoadHoleyFixedDoubleArrayElementCheckedNotHole* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadSignedIntDataViewElement(
    LoadSignedIntDataViewElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDoubleDataViewElement(
    LoadDoubleDataViewElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTypedArrayLength(
    LoadTypedArrayLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadSignedIntTypedArrayElement(
    LoadSignedIntTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadUnsignedIntTypedArrayElement(
    LoadUnsignedIntTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDoubleTypedArrayElement(
    LoadDoubleTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadSignedIntConstantTypedArrayElement(
    LoadSignedIntConstantTypedArrayElement* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitLoadUnsignedIntConstantTypedArrayElement(
    LoadUnsignedIntConstantTypedArrayElement* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDoubleConstantTypedArrayElement(
    LoadDoubleConstantTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadEnumCacheLength(
    LoadEnumCacheLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadGlobal(
    LoadGlobal* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadNamedGeneric(
    LoadNamedGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadNamedFromSuperGeneric(
    LoadNamedFromSuperGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMaybeGrowFastElements(
    MaybeGrowFastElements* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMigrateMapIfNeeded(
    MigrateMapIfNeeded* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetNamedGeneric(
    SetNamedGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDefineNamedOwnGeneric(
    DefineNamedOwnGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreInArrayLiteralGeneric(
    StoreInArrayLiteralGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreGlobal(
    StoreGlobal* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetKeyedGeneric(
    GetKeyedGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetKeyedGeneric(
    SetKeyedGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDefineKeyedOwnGeneric(
    DefineKeyedOwnGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitPhi(Phi* node,
                                             const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitRegisterInput(
    RegisterInput* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiSizedInt32(
    CheckedSmiSizedInt32* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetInt32Constant(GetInputAt(0))) {
    if (Smi::IsValid(cst.value())) {
      return ProcessResult::kRemove;
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagInt32(
    CheckedSmiTagInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagUint32(
    CheckedSmiTagUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagIntPtr(
    CheckedSmiTagIntPtr* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagInt32(
    UnsafeSmiTagInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagUint32(
    UnsafeSmiTagUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagIntPtr(
    UnsafeSmiTagIntPtr* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedInternalizedString(
    CheckedInternalizedString* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedObjectToIndex(
    CheckedObjectToIndex* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedInt32ToUint32(
    CheckedInt32ToUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedIntPtrToUint32(
    CheckedIntPtrToUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeInt32ToUint32(
    UnsafeInt32ToUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedUint32ToInt32(
    CheckedUint32ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedIntPtrToInt32(
    CheckedIntPtrToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeInt32ToFloat64(
    ChangeInt32ToFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeUint32ToFloat64(
    ChangeUint32ToFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeIntPtrToFloat64(
    ChangeIntPtrToFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedHoleyFloat64ToInt32(
    CheckedHoleyFloat64ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedHoleyFloat64ToUint32(
    CheckedHoleyFloat64ToUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTruncateUint32ToInt32(
    TruncateUint32ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTruncateHoleyFloat64ToInt32(
    TruncateHoleyFloat64ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeUint32ToInt32(
    UnsafeUint32ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeHoleyFloat64ToInt32(
    UnsafeHoleyFloat64ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ToUint8Clamped(
    Int32ToUint8Clamped* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUint32ToUint8Clamped(
    Uint32ToUint8Clamped* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToUint8Clamped(
    Float64ToUint8Clamped* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedNumberToUint8Clamped(
    CheckedNumberToUint8Clamped* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ToNumber(
    Int32ToNumber* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  auto cst = reducer_.TryGetInt32Constant(GetInputAt(0));
  if (cst.has_value() && Smi::IsValid(cst.value())) {
    return ReplaceWith(reducer_.GetSmiConstant(cst.value()));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUint32ToNumber(
    Uint32ToNumber* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32CountLeadingZeros(
    Int32CountLeadingZeros* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTaggedCountLeadingZeros(
    TaggedCountLeadingZeros* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64CountLeadingZeros(
    Float64CountLeadingZeros* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitIntPtrToBoolean(
    IntPtrToBoolean* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitIntPtrToNumber(
    IntPtrToNumber* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToTagged(
    Float64ToTagged* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToHeapNumberForField(
    Float64ToHeapNumberForField* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64ToTagged(
    HoleyFloat64ToTagged* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagFloat64(
    CheckedSmiTagFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#define UNTAGGING_CASE(Node, Repr, Type)                                     \
  ProcessResult MaglevGraphOptimizer::Visit##Node(                           \
      Node* node, const ProcessingState& state) {                            \
    if (ValueNode* input = GetUntaggedValueWithRepresentation(               \
            GetInputAt(0), UseRepresentation::k##Repr, NodeType::k##Type)) { \
      return ReplaceWith(input);                                             \
    }                                                                        \
    return ProcessResult::kContinue;                                         \
  }
UNTAGGING_CASE(UnsafeSmiUntag, Int32, Number)
UNTAGGING_CASE(CheckedNumberToInt32, Int32, Number)
UNTAGGING_CASE(TruncateCheckedNumberOrOddballToInt32, TruncatedInt32,
               NumberOrOddball)
UNTAGGING_CASE(TruncateUnsafeNumberOrOddballToInt32, TruncatedInt32,
               NumberOrOddball)
UNTAGGING_CASE(CheckedNumberOrOddballToFloat64, Float64, NumberOrOddball)
UNTAGGING_CASE(UncheckedNumberOrOddballToFloat64, Float64, NumberOrOddball)
UNTAGGING_CASE(CheckedNumberOrOddballToHoleyFloat64, HoleyFloat64,
               NumberOrOddball)
#undef UNTAGGING_CASE
ProcessResult MaglevGraphOptimizer::VisitCheckedSmiUntag(
    CheckedSmiUntag* node, const ProcessingState& state) {
  if (ValueNode* input = GetUntaggedValueWithRepresentation(
          GetInputAt(0), UseRepresentation::kInt32, NodeType::kNumber)) {
    if (SmiValuesAre31Bits()) {
      // When the graph builder introduced the CheckedSmiUntag, it also recorded
      // in the alternatives that its input was a known Smi from this point on.
      // This information could have been later used to avoid Smi checks when
      // using this input in contexts that require Smis (like storing the length
      // of an array for instance). We can thus bypass the CheckedSmiUntag, but
      // still need to keep a CheckSmi.
      // TODO(dmercadier): during graph building, record whether the "CheckSmi"
      // part of CheckSmiUntag is useful or not.
      ReduceResult result = reducer_.BuildCheckedSmiSizedInt32(input);
      CHECK(result.IsDone());
    }
    return ReplaceWith(input);
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedHoleyFloat64ToFloat64(
    CheckedHoleyFloat64ToFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64ToMaybeNanFloat64(
    HoleyFloat64ToMaybeNanFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#ifdef V8_ENABLE_UNDEFINED_DOUBLE

ProcessResult MaglevGraphOptimizer::VisitFloat64ToHoleyFloat64(
    Float64ToHoleyFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConvertHoleNanToUndefinedNan(
    ConvertHoleNanToUndefinedNan* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64IsUndefinedOrHole(
    HoleyFloat64IsUndefinedOrHole* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::
    VisitLoadHoleyFixedDoubleArrayElementCheckedNotUndefinedOrHole(
        LoadHoleyFixedDoubleArrayElementCheckedNotUndefinedOrHole* node,
        const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#else

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64IsHole(
    HoleyFloat64IsHole* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#endif  // V8_ENABLE_UNDEFINED_DOUBLE

ProcessResult MaglevGraphOptimizer::VisitLogicalNot(
    LogicalNot* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetPendingMessage(
    SetPendingMessage* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringAt(
    StringAt* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringEqual(
    StringEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringLength(
    StringLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringConcat(
    StringConcat* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSeqOneByteStringAt(
    SeqOneByteStringAt* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConsStringMap(
    ConsStringMap* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnwrapStringWrapper(
    UnwrapStringWrapper* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToBoolean(
    ToBoolean* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToBooleanLogicalNot(
    ToBooleanLogicalNot* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAllocateElementsArray(
    AllocateElementsArray* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTaggedEqual(
    TaggedEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTaggedNotEqual(
    TaggedNotEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTestInstanceOf(
    TestInstanceOf* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTestUndetectable(
    TestUndetectable* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTestTypeOf(
    TestTypeOf* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToName(ToName* node,
                                                const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToNumberOrNumeric(
    ToNumberOrNumeric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToObject(
    ToObject* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToString(
    ToString* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTransitionElementsKind(
    TransitionElementsKind* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitNumberToString(
    NumberToString* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUpdateJSArrayLength(
    UpdateJSArrayLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitVirtualObject(
    VirtualObject* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetContinuationPreservedEmbedderData(
    GetContinuationPreservedEmbedderData* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#define CONSTANT_CASE(Node)                        \
  ProcessResult MaglevGraphOptimizer::Visit##Node( \
      Node* node, const ProcessingState& state) {  \
    return ProcessResult::kContinue;               \
  }
CONSTANT_VALUE_NODE_LIST(CONSTANT_CASE)
#undef CONSTANT_CASE

ProcessResult MaglevGraphOptimizer::VisitInt32AbsWithOverflow(
    Int32AbsWithOverflow* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Add(
    Int32Add* node, const ProcessingState& state) {
  // TODO(victorgomes): TryFoldInt32Operation can emit a
  // Int32IncrementWithOverflow which needs an eager deopt point. We need to
  // propagate this information and we can add a non-deopting version of the
  // increment.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Subtract(
    Int32Subtract* node, const ProcessingState& state) {
  // TODO(victorgomes): TryFoldInt32Operation can emit a
  // Int32DecrementWithOverflow which needs an eager deopt point. We need to
  // propagate this information and we can add a non-deopting version of the
  // increment.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Multiply(
    Int32Multiply* node, const ProcessingState& state) {
  // TODO(victorgomes): TryFoldInt32Operation can emit a CheckInt32Condition
  // which needs an eager deopt point. We need to propagate this information.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32MultiplyOverflownBits(
    Int32MultiplyOverflownBits* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Divide(
    Int32Divide* node, const ProcessingState& state) {
  // TODO(victorgomes): TryFoldInt32Operation can emit a CheckInt32Condition
  // which needs an eager deopt point. We need to propagate this information
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32AddWithOverflow(
    Int32AddWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kAdd>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32SubtractWithOverflow(
    Int32SubtractWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kSubtract>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32MultiplyWithOverflow(
    Int32MultiplyWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kMultiply>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32DivideWithOverflow(
    Int32DivideWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kDivide>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ModulusWithOverflow(
    Int32ModulusWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kModulus>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseAnd(
    Int32BitwiseAnd* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kBitwiseAnd>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseOr(
    Int32BitwiseOr* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kBitwiseOr>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseXor(
    Int32BitwiseXor* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kBitwiseXor>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ShiftLeft(
    Int32ShiftLeft* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kShiftLeft>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ShiftRight(
    Int32ShiftRight* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kShiftRight>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ShiftRightLogical(
    Int32ShiftRightLogical* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kShiftRightLogical>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseNot(
    Int32BitwiseNot* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kBitwiseNot>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32NegateWithOverflow(
    Int32NegateWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kNegate>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32IncrementWithOverflow(
    Int32IncrementWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kIncrement>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32DecrementWithOverflow(
    Int32DecrementWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kDecrement>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Compare(
    Int32Compare* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ToBoolean(
    Int32ToBoolean* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Abs(
    Float64Abs* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Add(
    Float64Add* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kAdd>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Subtract(
    Float64Subtract* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kSubtract>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Multiply(
    Float64Multiply* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kMultiply>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Divide(
    Float64Divide* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kDivide>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Exponentiate(
    Float64Exponentiate* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kExponentiate>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Modulus(
    Float64Modulus* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kModulus>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Negate(
    Float64Negate* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kNegate>());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Round(
    Float64Round* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Compare(
    Float64Compare* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToBoolean(
    Float64ToBoolean* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Ieee754Unary(
    Float64Ieee754Unary* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Ieee754Binary(
    Float64Ieee754Binary* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Sqrt(
    Float64Sqrt* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiIncrement(
    CheckedSmiIncrement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiDecrement(
    CheckedSmiDecrement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericAdd(
    GenericAdd* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericSubtract(
    GenericSubtract* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericMultiply(
    GenericMultiply* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericDivide(
    GenericDivide* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericModulus(
    GenericModulus* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericExponentiate(
    GenericExponentiate* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseAnd(
    GenericBitwiseAnd* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseOr(
    GenericBitwiseOr* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseXor(
    GenericBitwiseXor* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericShiftLeft(
    GenericShiftLeft* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericShiftRight(
    GenericShiftRight* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericShiftRightLogical(
    GenericShiftRightLogical* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseNot(
    GenericBitwiseNot* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericNegate(
    GenericNegate* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericIncrement(
    GenericIncrement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericDecrement(
    GenericDecrement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericEqual(
    GenericEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericStrictEqual(
    GenericStrictEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericLessThan(
    GenericLessThan* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericLessThanOrEqual(
    GenericLessThanOrEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericGreaterThan(
    GenericGreaterThan* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericGreaterThanOrEqual(
    GenericGreaterThanOrEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBuiltinStringFromCharCode(
    BuiltinStringFromCharCode* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitBuiltinStringPrototypeCharCodeOrCodePointAt(
    BuiltinStringPrototypeCharCodeOrCodePointAt* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBuiltinSeqOneByteStringCharCodeAt(
    BuiltinSeqOneByteStringCharCodeAt* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateFastArrayElements(
    CreateFastArrayElements* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitNewConsString(
    NewConsString* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMapPrototypeGet(
    MapPrototypeGet* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMapPrototypeGetInt32Key(
    MapPrototypeGetInt32Key* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetPrototypeHas(
    SetPrototypeHas* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAbort(Abort* node,
                                               const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReturn(Return* node,
                                                const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDeopt(Deopt* node,
                                               const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrow(Throw* node,
                                               const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSwitch(Switch* node,
                                                const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfSmi(
    BranchIfSmi* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfRootConstant(
    BranchIfRootConstant* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfToBooleanTrue(
    BranchIfToBooleanTrue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfInt32ToBooleanTrue(
    BranchIfInt32ToBooleanTrue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfIntPtrToBooleanTrue(
    BranchIfIntPtrToBooleanTrue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64ToBooleanTrue(
    BranchIfFloat64ToBooleanTrue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#ifdef V8_ENABLE_UNDEFINED_DOUBLE

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64IsUndefinedOrHole(
    BranchIfFloat64IsUndefinedOrHole* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#endif

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64IsHole(
    BranchIfFloat64IsHole* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfReferenceEqual(
    BranchIfReferenceEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfInt32Compare(
    BranchIfInt32Compare* node, const ProcessingState& state) {
  if (auto result = reducer_.TryFoldInt32CompareOperation(
          node->operation(), GetInputAt(0), GetInputAt(1))) {
    FoldBranch(state.block(), node, result.value());
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfUint32Compare(
    BranchIfUint32Compare* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64Compare(
    BranchIfFloat64Compare* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfUndefinedOrNull(
    BranchIfUndefinedOrNull* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfUndetectable(
    BranchIfUndetectable* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfJSReceiver(
    BranchIfJSReceiver* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfTypeOf(
    BranchIfTypeOf* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitJump(Jump* node,
                                              const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckpointedJump(
    CheckpointedJump* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitJumpLoop(
    JumpLoop* node, const ProcessingState& state) {
  // We need to unwrap backedges of loop phis (since it's possible that they
  // weren't identities when the loop header was visited initially, but they
  // later became Identities while visiting the loop's body).
  BasicBlock* loop_header = node->target();
  if (!loop_header->has_phi()) return ProcessResult::kContinue;

  for (Phi* phi : *loop_header->phis()) {
    for (int i = 0; i < phi->input_count(); i++) {
      ValueNode* input = phi->input(i).node();
      if (!input) continue;
      phi->change_input(i, input->UnwrapIdentities());
    }
  }

  return ProcessResult::kContinue;
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
