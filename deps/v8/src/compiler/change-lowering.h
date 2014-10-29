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
class JSGraph;
class Linkage;
class MachineOperatorBuilder;

class ChangeLowering FINAL : public Reducer {
 public:
  ChangeLowering(JSGraph* jsgraph, Linkage* linkage)
      : jsgraph_(jsgraph), linkage_(linkage) {}
  virtual ~ChangeLowering();

  virtual Reduction Reduce(Node* node) OVERRIDE;

 private:
  Node* HeapNumberValueIndexConstant();
  Node* SmiMaxValueConstant();
  Node* SmiShiftBitsConstant();

  Node* AllocateHeapNumberWithValue(Node* value, Node* control);
  Node* ChangeSmiToInt32(Node* value);
  Node* LoadHeapNumberValue(Node* value, Node* control);

  Reduction ChangeBitToBool(Node* val, Node* control);
  Reduction ChangeBoolToBit(Node* val);
  Reduction ChangeFloat64ToTagged(Node* val, Node* control);
  Reduction ChangeInt32ToTagged(Node* val, Node* control);
  Reduction ChangeTaggedToFloat64(Node* val, Node* control);
  Reduction ChangeTaggedToUI32(Node* val, Node* control, Signedness signedness);
  Reduction ChangeUint32ToTagged(Node* val, Node* control);

  Graph* graph() const;
  Isolate* isolate() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Linkage* linkage() const { return linkage_; }
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;

  JSGraph* jsgraph_;
  Linkage* linkage_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CHANGE_LOWERING_H_
