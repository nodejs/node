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
  void ProcessNode(Node* node, Node** current_effect, Node** control);

  struct ValueEffectControl {
    Node* value;
    Node* effect;
    Node* control;
    ValueEffectControl(Node* value, Node* effect, Node* control)
        : value(value), effect(effect), control(control) {}
  };

  bool TryWireInStateEffect(Node* node, Node** effect, Node** control);
  ValueEffectControl LowerTypeGuard(Node* node, Node* effect, Node* control);
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
  ValueEffectControl LowerChangeTaggedToFloat64(Node* node, Node* effect,
                                                Node* control);
  ValueEffectControl LowerTruncateTaggedToWord32(Node* node, Node* effect,
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
  ValueEffectControl AllocateHeapNumberWithValue(Node* node, Node* effect,
                                                 Node* control);

  Node* ChangeInt32ToSmi(Node* value);
  Node* ChangeUint32ToSmi(Node* value);
  Node* ChangeInt32ToFloat64(Node* value);
  Node* ChangeUint32ToFloat64(Node* value);
  Node* ChangeSmiToInt32(Node* value);
  Node* ObjectIsSmi(Node* value);

  Node* SmiMaxValueConstant();
  Node* SmiShiftBitsConstant();

  JSGraph* jsgraph() const { return js_graph_; }
  Graph* graph() const;
  Schedule* schedule() const { return schedule_; }
  Zone* temp_zone() const { return temp_zone_; }
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() const;
  MachineOperatorBuilder* machine() const;

  JSGraph* js_graph_;
  Schedule* schedule_;
  Zone* temp_zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_EFFECT_CONTROL_LINEARIZER_H_
