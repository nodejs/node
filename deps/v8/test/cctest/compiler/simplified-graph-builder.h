// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_SIMPLIFIED_GRAPH_BUILDER_H_
#define V8_CCTEST_COMPILER_SIMPLIFIED_GRAPH_BUILDER_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-builder.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/simplified-operator.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/call-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

class SimplifiedGraphBuilder : public GraphBuilder {
 public:
  SimplifiedGraphBuilder(Graph* graph, CommonOperatorBuilder* common,
                         MachineOperatorBuilder* machine,
                         SimplifiedOperatorBuilder* simplified);
  virtual ~SimplifiedGraphBuilder() {}

  Zone* zone() const { return graph()->zone(); }
  Isolate* isolate() const { return zone()->isolate(); }
  CommonOperatorBuilder* common() const { return common_; }
  MachineOperatorBuilder* machine() const { return machine_; }
  SimplifiedOperatorBuilder* simplified() const { return simplified_; }

  // Initialize graph and builder.
  void Begin(int num_parameters);

  void Return(Node* value);

  // Close the graph.
  void End();

  Node* PointerConstant(void* value) {
    intptr_t intptr_value = reinterpret_cast<intptr_t>(value);
    return kPointerSize == 8 ? NewNode(common()->Int64Constant(intptr_value))
                             : Int32Constant(static_cast<int>(intptr_value));
  }
  Node* Int32Constant(int32_t value) {
    return NewNode(common()->Int32Constant(value));
  }
  Node* HeapConstant(Handle<HeapObject> object) {
    Unique<HeapObject> val = Unique<HeapObject>::CreateUninitialized(object);
    return NewNode(common()->HeapConstant(val));
  }

  Node* BooleanNot(Node* a) { return NewNode(simplified()->BooleanNot(), a); }

  Node* NumberEqual(Node* a, Node* b) {
    return NewNode(simplified()->NumberEqual(), a, b);
  }
  Node* NumberLessThan(Node* a, Node* b) {
    return NewNode(simplified()->NumberLessThan(), a, b);
  }
  Node* NumberLessThanOrEqual(Node* a, Node* b) {
    return NewNode(simplified()->NumberLessThanOrEqual(), a, b);
  }
  Node* NumberAdd(Node* a, Node* b) {
    return NewNode(simplified()->NumberAdd(), a, b);
  }
  Node* NumberSubtract(Node* a, Node* b) {
    return NewNode(simplified()->NumberSubtract(), a, b);
  }
  Node* NumberMultiply(Node* a, Node* b) {
    return NewNode(simplified()->NumberMultiply(), a, b);
  }
  Node* NumberDivide(Node* a, Node* b) {
    return NewNode(simplified()->NumberDivide(), a, b);
  }
  Node* NumberModulus(Node* a, Node* b) {
    return NewNode(simplified()->NumberModulus(), a, b);
  }
  Node* NumberToInt32(Node* a) {
    return NewNode(simplified()->NumberToInt32(), a);
  }
  Node* NumberToUint32(Node* a) {
    return NewNode(simplified()->NumberToUint32(), a);
  }

  Node* StringEqual(Node* a, Node* b) {
    return NewNode(simplified()->StringEqual(), a, b);
  }
  Node* StringLessThan(Node* a, Node* b) {
    return NewNode(simplified()->StringLessThan(), a, b);
  }
  Node* StringLessThanOrEqual(Node* a, Node* b) {
    return NewNode(simplified()->StringLessThanOrEqual(), a, b);
  }
  Node* StringAdd(Node* a, Node* b) {
    return NewNode(simplified()->StringAdd(), a, b);
  }

  Node* ChangeTaggedToInt32(Node* a) {
    return NewNode(simplified()->ChangeTaggedToInt32(), a);
  }
  Node* ChangeTaggedToUint32(Node* a) {
    return NewNode(simplified()->ChangeTaggedToUint32(), a);
  }
  Node* ChangeTaggedToFloat64(Node* a) {
    return NewNode(simplified()->ChangeTaggedToFloat64(), a);
  }
  Node* ChangeInt32ToTagged(Node* a) {
    return NewNode(simplified()->ChangeInt32ToTagged(), a);
  }
  Node* ChangeUint32ToTagged(Node* a) {
    return NewNode(simplified()->ChangeUint32ToTagged(), a);
  }
  Node* ChangeFloat64ToTagged(Node* a) {
    return NewNode(simplified()->ChangeFloat64ToTagged(), a);
  }
  Node* ChangeBoolToBit(Node* a) {
    return NewNode(simplified()->ChangeBoolToBit(), a);
  }
  Node* ChangeBitToBool(Node* a) {
    return NewNode(simplified()->ChangeBitToBool(), a);
  }

  Node* LoadField(const FieldAccess& access, Node* object) {
    return NewNode(simplified()->LoadField(access), object);
  }
  Node* StoreField(const FieldAccess& access, Node* object, Node* value) {
    return NewNode(simplified()->StoreField(access), object, value);
  }
  Node* LoadElement(const ElementAccess& access, Node* object, Node* index,
                    Node* length) {
    return NewNode(simplified()->LoadElement(access), object, index, length);
  }
  Node* StoreElement(const ElementAccess& access, Node* object, Node* index,
                     Node* length, Node* value) {
    return NewNode(simplified()->StoreElement(access), object, index, length,
                   value);
  }

 protected:
  virtual Node* MakeNode(const Operator* op, int value_input_count,
                         Node** value_inputs, bool incomplete) FINAL;

 private:
  Node* effect_;
  Node* return_;
  CommonOperatorBuilder* common_;
  MachineOperatorBuilder* machine_;
  SimplifiedOperatorBuilder* simplified_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_SIMPLIFIED_GRAPH_BUILDER_H_
