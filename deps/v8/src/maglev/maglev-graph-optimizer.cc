// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-optimizer.h"

#include <optional>

#include "src/base/logging.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-reducer-inl.h"
#include "src/maglev/maglev-reducer.h"

namespace v8 {
namespace internal {
namespace maglev {

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

MaglevGraphOptimizer::MaglevGraphOptimizer(Graph* graph)
    : reducer_(this, graph), empty_known_node_aspects_(graph->zone()) {}

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

ProcessResult MaglevGraphOptimizer::VisitAssertInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckDynamicValue() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckInt32IsSmi() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckUint32IsSmi() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckIntPtrIsSmi() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckHoleyFloat64IsSmi() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckHeapObject() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckInt32Condition() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckCacheIndicesNotCleared() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckJSDataViewBounds() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckTypedArrayBounds() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckTypedArrayNotDetached() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMaps() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMapsWithMigrationAndDeopt() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMapsWithMigration() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMapsWithAlreadyLoadedMap() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckDetectableCallable() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckJSReceiverOrNullOrUndefined() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckNotHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckHoleyFloat64NotHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckNumber() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckSmi() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckString() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckSeqOneByteString() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckStringOrStringWrapper() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckStringOrOddball() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckSymbol() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckValue() {
  CheckValue* node = current_node()->Cast<CheckValue>();
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

ProcessResult MaglevGraphOptimizer::VisitCheckValueEqualsInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckFloat64SameValue() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckValueEqualsString() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckInstanceType() {
  // TODO(b/424157317): Optimize.
  CheckInstanceType* node = current_node()->Cast<CheckInstanceType>();
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

ProcessResult MaglevGraphOptimizer::VisitDead() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDebugBreak() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFunctionEntryStackCheck() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGeneratorStore() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTryOnStackReplacement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreMap() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreDoubleField() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitStoreFixedArrayElementWithWriteBarrier() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitStoreFixedArrayElementNoWriteBarrier() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFixedDoubleArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFloat64() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreIntTypedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreDoubleTypedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreIntConstantTypedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitStoreDoubleConstantTypedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreSignedIntDataViewElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreDoubleDataViewElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreTaggedFieldNoWriteBarrier() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreTaggedFieldWithWriteBarrier() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreContextSlotWithWriteBarrier() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitStoreTrustedPointerFieldWithWriteBarrier() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHandleNoHeapWritesInterrupt() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReduceInterruptBudgetForLoop() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReduceInterruptBudgetForReturn() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowReferenceErrorIfHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowSuperNotCalledIfHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowSuperAlreadyCalledIfNotHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowIfNotCallable() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowIfNotSuperConstructor() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTransitionElementsKindOrCheckMap() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitSetContinuationPreservedEmbedderData() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTransitionAndStoreArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConstantGapMove() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGapMove() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitIdentity() {
  // If a non-eager inlined function returns a tagged value, we substitute the
  // call with an Identity. The node is then removed from the graph here. All
  // references to it will be removed in this graph optimizer pass.
  return ProcessResult::kRemove;
}

ProcessResult MaglevGraphOptimizer::VisitAllocationBlock() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitArgumentsElements() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitArgumentsLength() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitRestLength() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCall() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallBuiltin() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallCPPBuiltin() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallForwardVarargs() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallRuntime() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallWithArrayLike() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallWithSpread() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallKnownApiFunction() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallKnownJSFunction() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReturnedValue() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallSelf() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConstruct() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckConstructResult() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckDerivedConstructResult() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConstructWithSpread() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConvertReceiver() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConvertHoleToUndefined() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateArrayLiteral() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateShallowArrayLiteral() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateObjectLiteral() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateShallowObjectLiteral() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateFunctionContext() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateClosure() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFastCreateClosure() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateRegExpLiteral() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDeleteProperty() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitEnsureWritableFastElements() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitExtendPropertiesBackingStore() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInlinedAllocation() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitForInPrepare() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitForInNext() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGeneratorRestoreRegister() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetIterator() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetSecondReturnedValue() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetTemplateObject() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHasInPrototypeChain() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInitialValue() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedField() {
  // TODO(b/424157317): Optimize.
  LoadTaggedField* node = current_node()->Cast<LoadTaggedField>();
  if (node->offset() == JSFunction::kFeedbackCellOffset) {
    if (auto input = GetInputAt(0)->TryCast<FastCreateClosure>()) {
      return ReplaceWith(reducer_.GetConstant(input->feedback_cell()));
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedFieldForProperty() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitLoadTaggedFieldForContextSlotNoCells() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedFieldForContextSlot() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDoubleField() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadFloat64() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedFieldByFieldIndex() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadFixedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadFixedDoubleArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadHoleyFixedDoubleArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitLoadHoleyFixedDoubleArrayElementCheckedNotHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadSignedIntDataViewElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDoubleDataViewElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTypedArrayLength() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadSignedIntTypedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadUnsignedIntTypedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDoubleTypedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitLoadSignedIntConstantTypedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitLoadUnsignedIntConstantTypedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDoubleConstantTypedArrayElement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadEnumCacheLength() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadGlobal() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadNamedGeneric() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadNamedFromSuperGeneric() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMaybeGrowFastElements() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMigrateMapIfNeeded() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetNamedGeneric() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDefineNamedOwnGeneric() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreInArrayLiteralGeneric() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreGlobal() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetKeyedGeneric() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetKeyedGeneric() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDefineKeyedOwnGeneric() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitPhi() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitRegisterInput() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiSizedInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagUint32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagIntPtr() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagUint32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagIntPtr() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedInternalizedString() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedObjectToIndex() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedInt32ToUint32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedIntPtrToUint32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeInt32ToUint32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedUint32ToInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedIntPtrToInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeInt32ToFloat64() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeUint32ToFloat64() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeIntPtrToFloat64() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedHoleyFloat64ToInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedHoleyFloat64ToUint32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitTruncateUnsafeNumberOrOddballToInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTruncateUint32ToInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTruncateHoleyFloat64ToInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeUint32ToInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeHoleyFloat64ToInt32() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ToUint8Clamped() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUint32ToUint8Clamped() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToUint8Clamped() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedNumberToUint8Clamped() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ToNumber() {
  // TODO(b/424157317): Optimize.
  auto cst = reducer_.TryGetInt32Constant(GetInputAt(0));
  if (cst.has_value() && Smi::IsValid(cst.value())) {
    return ReplaceWith(reducer_.GetSmiConstant(cst.value()));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUint32ToNumber() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32CountLeadingZeros() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTaggedCountLeadingZeros() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64CountLeadingZeros() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitIntPtrToBoolean() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitIntPtrToNumber() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToTagged() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToHeapNumberForField() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64ToTagged() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagFloat64() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#define UNTAGGING_CASE(Node, Repr, Type)                                     \
  ProcessResult MaglevGraphOptimizer::Visit##Node() {                        \
    if (ValueNode* input = GetUntaggedValueWithRepresentation(               \
            GetInputAt(0), UseRepresentation::k##Repr, NodeType::k##Type)) { \
      return ReplaceWith(input);                                             \
    }                                                                        \
    return ProcessResult::kContinue;                                         \
  }
UNTAGGING_CASE(CheckedSmiUntag, Int32, Number)
UNTAGGING_CASE(UnsafeSmiUntag, Int32, Number)
UNTAGGING_CASE(CheckedNumberToInt32, Int32, Number)
UNTAGGING_CASE(TruncateCheckedNumberOrOddballToInt32, TruncatedInt32,
               NumberOrOddball)
UNTAGGING_CASE(CheckedNumberOrOddballToFloat64, Float64, NumberOrOddball)
UNTAGGING_CASE(UncheckedNumberOrOddballToFloat64, Float64, NumberOrOddball)
UNTAGGING_CASE(CheckedNumberOrOddballToHoleyFloat64, HoleyFloat64,
               NumberOrOddball)
#undef UNTAGGING_CASE

ProcessResult MaglevGraphOptimizer::VisitCheckedHoleyFloat64ToFloat64() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64ToMaybeNanFloat64() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#ifdef V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE

ProcessResult MaglevGraphOptimizer::VisitFloat64ToHoleyFloat64() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConvertHoleNanToUndefinedNan() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64IsUndefinedOrHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::
    VisitLoadHoleyFixedDoubleArrayElementCheckedNotUndefinedOrHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#else

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64IsHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#endif  // V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE

ProcessResult MaglevGraphOptimizer::VisitLogicalNot() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetPendingMessage() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringAt() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringEqual() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringLength() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringConcat() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSeqOneByteStringAt() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConsStringMap() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnwrapStringWrapper() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToBoolean() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToBooleanLogicalNot() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAllocateElementsArray() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTaggedEqual() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTaggedNotEqual() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTestInstanceOf() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTestUndetectable() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTestTypeOf() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToName() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToNumberOrNumeric() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToObject() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToString() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTransitionElementsKind() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitNumberToString() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUpdateJSArrayLength() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitVirtualObject() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitGetContinuationPreservedEmbedderData() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#define CONSTANT_CASE(Node)                           \
  ProcessResult MaglevGraphOptimizer::Visit##Node() { \
    return ProcessResult::kContinue;                  \
  }
CONSTANT_VALUE_NODE_LIST(CONSTANT_CASE)
#undef CONSTANT_CASE

ProcessResult MaglevGraphOptimizer::VisitInt32AbsWithOverflow() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Add() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Subtract() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Multiply() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32MultiplyOverflownBits() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Divide() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32AddWithOverflow() {
  // TODO(b/424157317): Optimize.
  if (MaybeReduceResult result =
          reducer_.TryFoldInt32BinaryOperation<Operation::kAdd>(GetInputAt(0),
                                                                GetInputAt(1));
      result.IsDone()) {
    DCHECK(result.IsDoneWithValue());
    return ReplaceWith(reducer_.GetInt32(result.value()));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32SubtractWithOverflow() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32MultiplyWithOverflow() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32DivideWithOverflow() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ModulusWithOverflow() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseAnd() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseOr() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseXor() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ShiftLeft() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ShiftRight() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ShiftRightLogical() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseNot() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32NegateWithOverflow() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32IncrementWithOverflow() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32DecrementWithOverflow() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Compare() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ToBoolean() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Abs() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Add() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Subtract() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Multiply() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Divide() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Exponentiate() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Modulus() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Negate() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Round() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Compare() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToBoolean() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Ieee754Unary() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Ieee754Binary() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Sqrt() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiIncrement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiDecrement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericAdd() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericSubtract() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericMultiply() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericDivide() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericModulus() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericExponentiate() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseAnd() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseOr() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseXor() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericShiftLeft() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericShiftRight() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericShiftRightLogical() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseNot() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericNegate() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericIncrement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericDecrement() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericEqual() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericStrictEqual() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericLessThan() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericLessThanOrEqual() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericGreaterThan() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericGreaterThanOrEqual() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBuiltinStringFromCharCode() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitBuiltinStringPrototypeCharCodeOrCodePointAt() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBuiltinSeqOneByteStringCharCodeAt() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateFastArrayElements() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitNewConsString() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMapPrototypeGet() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMapPrototypeGetInt32Key() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetPrototypeHas() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAbort() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReturn() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDeopt() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSwitch() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfSmi() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfRootConstant() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfToBooleanTrue() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfInt32ToBooleanTrue() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfIntPtrToBooleanTrue() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64ToBooleanTrue() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#ifdef V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64IsUndefinedOrHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#endif

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64IsHole() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfReferenceEqual() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfInt32Compare() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfUint32Compare() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64Compare() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfUndefinedOrNull() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfUndetectable() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfJSReceiver() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfTypeOf() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitJump() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckpointedJump() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitJumpLoop() {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
