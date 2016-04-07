// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CHANGE_LOWERING_H_
#define V8_COMPILER_CHANGE_LOWERING_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
struct ElementAccess;
class JSGraph;
class Linkage;
class MachineOperatorBuilder;
class Operator;

class ChangeLowering final : public Reducer {
 public:
  explicit ChangeLowering(JSGraph* jsgraph) : jsgraph_(jsgraph) {}
  ~ChangeLowering() final;

  Reduction Reduce(Node* node) final;

 private:
  Node* HeapNumberValueIndexConstant();
  Node* SmiMaxValueConstant();
  Node* SmiShiftBitsConstant();

  Node* AllocateHeapNumberWithValue(Node* value, Node* control);
  Node* ChangeInt32ToFloat64(Node* value);
  Node* ChangeInt32ToSmi(Node* value);
  Node* ChangeSmiToFloat64(Node* value);
  Node* ChangeSmiToInt32(Node* value);
  Node* ChangeUint32ToFloat64(Node* value);
  Node* ChangeUint32ToSmi(Node* value);
  Node* LoadHeapNumberValue(Node* value, Node* control);
  Node* TestNotSmi(Node* value);

  Reduction ChangeBitToBool(Node* value, Node* control);
  Reduction ChangeBoolToBit(Node* value);
  Reduction ChangeFloat64ToTagged(Node* value, Node* control);
  Reduction ChangeInt32ToTagged(Node* value, Node* control);
  Reduction ChangeTaggedToFloat64(Node* value, Node* control);
  Reduction ChangeTaggedToUI32(Node* value, Node* control,
                               Signedness signedness);
  Reduction ChangeUint32ToTagged(Node* value, Node* control);

  Reduction LoadField(Node* node);
  Reduction StoreField(Node* node);
  Reduction LoadElement(Node* node);
  Reduction StoreElement(Node* node);
  Reduction Allocate(Node* node);

  Node* IsSmi(Node* value);
  Node* LoadHeapObjectMap(Node* object, Node* control);
  Node* LoadMapInstanceType(Node* map);

  Reduction ObjectIsNumber(Node* node);
  Reduction ObjectIsReceiver(Node* node);
  Reduction ObjectIsSmi(Node* node);

  Node* ComputeIndex(const ElementAccess& access, Node* const key);
  Graph* graph() const;
  Isolate* isolate() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;

  JSGraph* const jsgraph_;
  SetOncePointer<const Operator> allocate_heap_number_operator_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CHANGE_LOWERING_H_
