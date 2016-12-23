// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_EFFECT_CONTROL_LINEARIZER_H_
#define V8_COMPILER_EFFECT_CONTROL_LINEARIZER_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

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

class EffectControlLinearizer {
 public:
  EffectControlLinearizer(JSGraph* graph, Schedule* schedule, Zone* temp_zone);

  void Run();

 private:
  void ProcessNode(Node* node, Node** frame_state, Node** effect,
                   Node** control);

  struct ValueEffectControl {
    Node* value;
    Node* effect;
    Node* control;
    ValueEffectControl(Node* value, Node* effect, Node* control)
        : value(value), effect(effect), control(control) {}
  };

  bool TryWireInStateEffect(Node* node, Node* frame_state, Node** effect,
                            Node** control);
  ValueEffectControl LowerChangeBitToTagged(Node* node, Node* effect,
                                            Node* control);
  ValueEffectControl LowerChangeInt31ToTaggedSigned(Node* node, Node* effect,
                                                    Node* control);
  ValueEffectControl LowerChangeInt32ToTagged(Node* node, Node* effect,
                                              Node* control);
  ValueEffectControl LowerChangeUint32ToTagged(Node* node, Node* effect,
                                               Node* control);
  ValueEffectControl LowerChangeFloat64ToTagged(Node* node, Node* effect,
                                                Node* control);
  ValueEffectControl LowerChangeTaggedSignedToInt32(Node* node, Node* effect,
                                                    Node* control);
  ValueEffectControl LowerChangeTaggedToBit(Node* node, Node* effect,
                                            Node* control);
  ValueEffectControl LowerChangeTaggedToInt32(Node* node, Node* effect,
                                              Node* control);
  ValueEffectControl LowerChangeTaggedToUint32(Node* node, Node* effect,
                                               Node* control);
  ValueEffectControl LowerCheckBounds(Node* node, Node* frame_state,
                                      Node* effect, Node* control);
  ValueEffectControl LowerCheckMaps(Node* node, Node* frame_state, Node* effect,
                                    Node* control);
  ValueEffectControl LowerCheckNumber(Node* node, Node* frame_state,
                                      Node* effect, Node* control);
  ValueEffectControl LowerCheckString(Node* node, Node* frame_state,
                                      Node* effect, Node* control);
  ValueEffectControl LowerCheckIf(Node* node, Node* frame_state, Node* effect,
                                  Node* control);
  ValueEffectControl LowerCheckHeapObject(Node* node, Node* frame_state,
                                          Node* effect, Node* control);
  ValueEffectControl LowerCheckedInt32Add(Node* node, Node* frame_state,
                                          Node* effect, Node* control);
  ValueEffectControl LowerCheckedInt32Sub(Node* node, Node* frame_state,
                                          Node* effect, Node* control);
  ValueEffectControl LowerCheckedInt32Div(Node* node, Node* frame_state,
                                          Node* effect, Node* control);
  ValueEffectControl LowerCheckedInt32Mod(Node* node, Node* frame_state,
                                          Node* effect, Node* control);
  ValueEffectControl LowerCheckedUint32Div(Node* node, Node* frame_state,
                                           Node* effect, Node* control);
  ValueEffectControl LowerCheckedUint32Mod(Node* node, Node* frame_state,
                                           Node* effect, Node* control);
  ValueEffectControl LowerCheckedInt32Mul(Node* node, Node* frame_state,
                                          Node* effect, Node* control);
  ValueEffectControl LowerCheckedInt32ToTaggedSigned(Node* node,
                                                     Node* frame_state,
                                                     Node* effect,
                                                     Node* control);
  ValueEffectControl LowerCheckedUint32ToInt32(Node* node, Node* frame_state,
                                               Node* effect, Node* control);
  ValueEffectControl LowerCheckedUint32ToTaggedSigned(Node* node,
                                                      Node* frame_state,
                                                      Node* effect,
                                                      Node* control);
  ValueEffectControl LowerCheckedFloat64ToInt32(Node* node, Node* frame_state,
                                                Node* effect, Node* control);
  ValueEffectControl LowerCheckedTaggedSignedToInt32(Node* node,
                                                     Node* frame_state,
                                                     Node* effect,
                                                     Node* control);
  ValueEffectControl LowerCheckedTaggedToInt32(Node* node, Node* frame_state,
                                               Node* effect, Node* control);
  ValueEffectControl LowerCheckedTaggedToFloat64(Node* node, Node* frame_state,
                                                 Node* effect, Node* control);
  ValueEffectControl LowerCheckedTaggedToTaggedSigned(Node* node,
                                                      Node* frame_state,
                                                      Node* effect,
                                                      Node* control);
  ValueEffectControl LowerChangeTaggedToFloat64(Node* node, Node* effect,
                                                Node* control);
  ValueEffectControl LowerTruncateTaggedToBit(Node* node, Node* effect,
                                              Node* control);
  ValueEffectControl LowerTruncateTaggedToFloat64(Node* node, Node* effect,
                                                  Node* control);
  ValueEffectControl LowerTruncateTaggedToWord32(Node* node, Node* effect,
                                                 Node* control);
  ValueEffectControl LowerCheckedTruncateTaggedToWord32(Node* node,
                                                        Node* frame_state,
                                                        Node* effect,
                                                        Node* control);
  ValueEffectControl LowerObjectIsCallable(Node* node, Node* effect,
                                           Node* control);
  ValueEffectControl LowerObjectIsNumber(Node* node, Node* effect,
                                         Node* control);
  ValueEffectControl LowerObjectIsReceiver(Node* node, Node* effect,
                                           Node* control);
  ValueEffectControl LowerObjectIsSmi(Node* node, Node* effect, Node* control);
  ValueEffectControl LowerObjectIsString(Node* node, Node* effect,
                                         Node* control);
  ValueEffectControl LowerObjectIsUndetectable(Node* node, Node* effect,
                                               Node* control);
  ValueEffectControl LowerArrayBufferWasNeutered(Node* node, Node* effect,
                                                 Node* control);
  ValueEffectControl LowerStringCharCodeAt(Node* node, Node* effect,
                                           Node* control);
  ValueEffectControl LowerStringFromCharCode(Node* node, Node* effect,
                                             Node* control);
  ValueEffectControl LowerStringFromCodePoint(Node* node, Node* effect,
                                              Node* control);
  ValueEffectControl LowerStringEqual(Node* node, Node* effect, Node* control);
  ValueEffectControl LowerStringLessThan(Node* node, Node* effect,
                                         Node* control);
  ValueEffectControl LowerStringLessThanOrEqual(Node* node, Node* effect,
                                                Node* control);
  ValueEffectControl LowerCheckFloat64Hole(Node* node, Node* frame_state,
                                           Node* effect, Node* control);
  ValueEffectControl LowerCheckTaggedHole(Node* node, Node* frame_state,
                                          Node* effect, Node* control);
  ValueEffectControl LowerConvertTaggedHoleToUndefined(Node* node, Node* effect,
                                                       Node* control);
  ValueEffectControl LowerPlainPrimitiveToNumber(Node* node, Node* effect,
                                                 Node* control);
  ValueEffectControl LowerPlainPrimitiveToWord32(Node* node, Node* effect,
                                                 Node* control);
  ValueEffectControl LowerPlainPrimitiveToFloat64(Node* node, Node* effect,
                                                  Node* control);
  ValueEffectControl LowerEnsureWritableFastElements(Node* node, Node* effect,
                                                     Node* control);
  ValueEffectControl LowerMaybeGrowFastElements(Node* node, Node* frame_state,
                                                Node* effect, Node* control);
  ValueEffectControl LowerTransitionElementsKind(Node* node, Node* effect,
                                                 Node* control);
  ValueEffectControl LowerLoadTypedElement(Node* node, Node* effect,
                                           Node* control);
  ValueEffectControl LowerStoreTypedElement(Node* node, Node* effect,
                                            Node* control);

  // Lowering of optional operators.
  ValueEffectControl LowerFloat64RoundUp(Node* node, Node* effect,
                                         Node* control);
  ValueEffectControl LowerFloat64RoundDown(Node* node, Node* effect,
                                           Node* control);
  ValueEffectControl LowerFloat64RoundTruncate(Node* node, Node* effect,
                                               Node* control);

  ValueEffectControl AllocateHeapNumberWithValue(Node* node, Node* effect,
                                                 Node* control);
  ValueEffectControl BuildCheckedFloat64ToInt32(CheckForMinusZeroMode mode,
                                                Node* value, Node* frame_state,
                                                Node* effect, Node* control);
  ValueEffectControl BuildCheckedHeapNumberOrOddballToFloat64(
      CheckTaggedInputMode mode, Node* value, Node* frame_state, Node* effect,
      Node* control);
  ValueEffectControl LowerStringComparison(Callable const& callable, Node* node,
                                           Node* effect, Node* control);

  Node* ChangeInt32ToSmi(Node* value);
  Node* ChangeUint32ToSmi(Node* value);
  Node* ChangeInt32ToFloat64(Node* value);
  Node* ChangeUint32ToFloat64(Node* value);
  Node* ChangeSmiToInt32(Node* value);
  Node* ObjectIsSmi(Node* value);

  Node* SmiMaxValueConstant();
  Node* SmiShiftBitsConstant();

  Factory* factory() const;
  Isolate* isolate() const;
  JSGraph* jsgraph() const { return js_graph_; }
  Graph* graph() const;
  Schedule* schedule() const { return schedule_; }
  Zone* temp_zone() const { return temp_zone_; }
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() const;
  MachineOperatorBuilder* machine() const;

  Operator const* ToNumberOperator();

  JSGraph* js_graph_;
  Schedule* schedule_;
  Zone* temp_zone_;
  RegionObservability region_observability_ = RegionObservability::kObservable;

  SetOncePointer<Operator const> to_number_operator_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_EFFECT_CONTROL_LINEARIZER_H_
