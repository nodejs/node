// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_EFFECT_CONTROL_LINEARIZER_H_
#define V8_COMPILER_EFFECT_CONTROL_LINEARIZER_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-assembler.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Callable;
class Zone;

namespace compiler {

class CommonOperatorBuilder;
class SimplifiedOperatorBuilder;
class MachineOperatorBuilder;
class JSGraph;
class Graph;
class Schedule;
class SourcePositionTable;

class V8_EXPORT_PRIVATE EffectControlLinearizer {
 public:
  enum MaskArrayIndexEnable { kDoNotMaskArrayIndex, kMaskArrayIndex };

  EffectControlLinearizer(JSGraph* graph, Schedule* schedule, Zone* temp_zone,
                          SourcePositionTable* source_positions,
                          MaskArrayIndexEnable mask_array_index);

  void Run();

 private:
  void ProcessNode(Node* node, Node** frame_state, Node** effect,
                   Node** control);

  bool TryWireInStateEffect(Node* node, Node* frame_state, Node** effect,
                            Node** control);
  Node* LowerChangeBitToTagged(Node* node);
  Node* LowerChangeInt31ToTaggedSigned(Node* node);
  Node* LowerChangeInt32ToTagged(Node* node);
  Node* LowerChangeUint32ToTagged(Node* node);
  Node* LowerChangeFloat64ToTagged(Node* node);
  Node* LowerChangeFloat64ToTaggedPointer(Node* node);
  Node* LowerChangeTaggedSignedToInt32(Node* node);
  Node* LowerChangeTaggedToBit(Node* node);
  Node* LowerChangeTaggedToInt32(Node* node);
  Node* LowerChangeTaggedToUint32(Node* node);
  Node* LowerChangeTaggedToTaggedSigned(Node* node);
  Node* LowerCheckBounds(Node* node, Node* frame_state);
  Node* LowerMaskIndexWithBound(Node* node);
  Node* LowerCheckInternalizedString(Node* node, Node* frame_state);
  void LowerCheckMaps(Node* node, Node* frame_state);
  Node* LowerCompareMaps(Node* node);
  Node* LowerCheckNumber(Node* node, Node* frame_state);
  Node* LowerCheckReceiver(Node* node, Node* frame_state);
  Node* LowerCheckString(Node* node, Node* frame_state);
  Node* LowerCheckSeqString(Node* node, Node* frame_state);
  Node* LowerCheckSymbol(Node* node, Node* frame_state);
  void LowerCheckIf(Node* node, Node* frame_state);
  Node* LowerCheckedInt32Add(Node* node, Node* frame_state);
  Node* LowerCheckedInt32Sub(Node* node, Node* frame_state);
  Node* LowerCheckedInt32Div(Node* node, Node* frame_state);
  Node* LowerCheckedInt32Mod(Node* node, Node* frame_state);
  Node* LowerCheckedUint32Div(Node* node, Node* frame_state);
  Node* LowerCheckedUint32Mod(Node* node, Node* frame_state);
  Node* LowerCheckedInt32Mul(Node* node, Node* frame_state);
  Node* LowerCheckedInt32ToTaggedSigned(Node* node, Node* frame_state);
  Node* LowerCheckedUint32ToInt32(Node* node, Node* frame_state);
  Node* LowerCheckedUint32ToTaggedSigned(Node* node, Node* frame_state);
  Node* LowerCheckedFloat64ToInt32(Node* node, Node* frame_state);
  Node* LowerCheckedTaggedSignedToInt32(Node* node, Node* frame_state);
  Node* LowerCheckedTaggedToInt32(Node* node, Node* frame_state);
  Node* LowerCheckedTaggedToFloat64(Node* node, Node* frame_state);
  Node* LowerCheckedTaggedToTaggedSigned(Node* node, Node* frame_state);
  Node* LowerCheckedTaggedToTaggedPointer(Node* node, Node* frame_state);
  Node* LowerChangeTaggedToFloat64(Node* node);
  void TruncateTaggedPointerToBit(Node* node, GraphAssemblerLabel<1>* done);
  Node* LowerTruncateTaggedToBit(Node* node);
  Node* LowerTruncateTaggedPointerToBit(Node* node);
  Node* LowerTruncateTaggedToFloat64(Node* node);
  Node* LowerTruncateTaggedToWord32(Node* node);
  Node* LowerCheckedTruncateTaggedToWord32(Node* node, Node* frame_state);
  Node* LowerAllocate(Node* node);
  Node* LowerNumberToString(Node* node);
  Node* LowerObjectIsArrayBufferView(Node* node);
  Node* LowerObjectIsBigInt(Node* node);
  Node* LowerObjectIsCallable(Node* node);
  Node* LowerObjectIsConstructor(Node* node);
  Node* LowerObjectIsDetectableCallable(Node* node);
  Node* LowerObjectIsMinusZero(Node* node);
  Node* LowerObjectIsNaN(Node* node);
  Node* LowerObjectIsNonCallable(Node* node);
  Node* LowerObjectIsNumber(Node* node);
  Node* LowerObjectIsReceiver(Node* node);
  Node* LowerObjectIsSmi(Node* node);
  Node* LowerObjectIsString(Node* node);
  Node* LowerObjectIsSymbol(Node* node);
  Node* LowerObjectIsUndetectable(Node* node);
  Node* LowerNumberIsFloat64Hole(Node* node);
  Node* LowerArgumentsFrame(Node* node);
  Node* LowerArgumentsLength(Node* node);
  Node* LowerNewDoubleElements(Node* node);
  Node* LowerNewSmiOrObjectElements(Node* node);
  Node* LowerNewArgumentsElements(Node* node);
  Node* LowerNewConsString(Node* node);
  Node* LowerArrayBufferWasNeutered(Node* node);
  Node* LowerSameValue(Node* node);
  Node* LowerDeadValue(Node* node);
  Node* LowerStringToNumber(Node* node);
  Node* LowerStringCharAt(Node* node);
  Node* LowerStringCharCodeAt(Node* node);
  Node* LowerSeqStringCharCodeAt(Node* node);
  Node* LowerStringCodePointAt(Node* node);
  Node* LowerSeqStringCodePointAt(Node* node, UnicodeEncoding encoding);
  Node* LowerStringToLowerCaseIntl(Node* node);
  Node* LowerStringToUpperCaseIntl(Node* node);
  Node* LowerStringFromCharCode(Node* node);
  Node* LowerStringFromCodePoint(Node* node);
  Node* LowerStringIndexOf(Node* node);
  Node* LowerStringLength(Node* node);
  Node* LowerStringEqual(Node* node);
  Node* LowerStringLessThan(Node* node);
  Node* LowerStringLessThanOrEqual(Node* node);
  Node* LowerCheckFloat64Hole(Node* node, Node* frame_state);
  Node* LowerCheckNotTaggedHole(Node* node, Node* frame_state);
  Node* LowerConvertTaggedHoleToUndefined(Node* node);
  void LowerCheckEqualsInternalizedString(Node* node, Node* frame_state);
  void LowerCheckEqualsSymbol(Node* node, Node* frame_state);
  Node* LowerTypeOf(Node* node);
  Node* LowerClassOf(Node* node);
  Node* LowerToBoolean(Node* node);
  Node* LowerPlainPrimitiveToNumber(Node* node);
  Node* LowerPlainPrimitiveToWord32(Node* node);
  Node* LowerPlainPrimitiveToFloat64(Node* node);
  Node* LowerEnsureWritableFastElements(Node* node);
  Node* LowerMaybeGrowFastElements(Node* node, Node* frame_state);
  void LowerTransitionElementsKind(Node* node);
  Node* LowerLoadFieldByIndex(Node* node);
  Node* LowerLoadTypedElement(Node* node);
  void LowerStoreTypedElement(Node* node);
  void LowerStoreSignedSmallElement(Node* node);
  Node* LowerFindOrderedHashMapEntry(Node* node);
  Node* LowerFindOrderedHashMapEntryForInt32Key(Node* node);
  void LowerTransitionAndStoreElement(Node* node);
  void LowerTransitionAndStoreNumberElement(Node* node);
  void LowerTransitionAndStoreNonNumberElement(Node* node);
  void LowerRuntimeAbort(Node* node);
  Node* LowerConvertReceiver(Node* node);

  // Lowering of optional operators.
  Maybe<Node*> LowerFloat64RoundUp(Node* node);
  Maybe<Node*> LowerFloat64RoundDown(Node* node);
  Maybe<Node*> LowerFloat64RoundTiesEven(Node* node);
  Maybe<Node*> LowerFloat64RoundTruncate(Node* node);

  Node* AllocateHeapNumberWithValue(Node* node);
  Node* BuildCheckedFloat64ToInt32(CheckForMinusZeroMode mode,
                                   const VectorSlotPair& feedback, Node* value,
                                   Node* frame_state);
  Node* BuildCheckedHeapNumberOrOddballToFloat64(CheckTaggedInputMode mode,
                                                 const VectorSlotPair& feedback,
                                                 Node* value,
                                                 Node* frame_state);
  Node* BuildFloat64RoundDown(Node* value);
  Node* ComputeIntegerHash(Node* value);
  Node* LowerStringComparison(Callable const& callable, Node* node);
  Node* IsElementsKindGreaterThan(Node* kind, ElementsKind reference_kind);

  Node* ChangeInt32ToSmi(Node* value);
  Node* ChangeIntPtrToInt32(Node* value);
  Node* ChangeUint32ToUintPtr(Node* value);
  Node* ChangeUint32ToSmi(Node* value);
  Node* ChangeSmiToIntPtr(Node* value);
  Node* ChangeSmiToInt32(Node* value);
  Node* ObjectIsSmi(Node* value);
  Node* LoadFromString(Node* receiver, Node* position, Node* is_one_byte);

  Node* SmiMaxValueConstant();
  Node* SmiShiftBitsConstant();
  void TransitionElementsTo(Node* node, Node* array, ElementsKind from,
                            ElementsKind to);

  Factory* factory() const;
  Isolate* isolate() const;
  JSGraph* jsgraph() const { return js_graph_; }
  Graph* graph() const;
  Schedule* schedule() const { return schedule_; }
  Zone* temp_zone() const { return temp_zone_; }
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() const;
  MachineOperatorBuilder* machine() const;

  GraphAssembler* gasm() { return &graph_assembler_; }

  JSGraph* js_graph_;
  Schedule* schedule_;
  Zone* temp_zone_;
  MaskArrayIndexEnable mask_array_index_;
  RegionObservability region_observability_ = RegionObservability::kObservable;
  SourcePositionTable* source_positions_;
  GraphAssembler graph_assembler_;
  Node* frame_state_zapper_;  // For tracking down compiler::Node::New crashes.
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_EFFECT_CONTROL_LINEARIZER_H_
