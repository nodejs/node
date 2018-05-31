// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ALLOCATION_BUILDER_H_
#define V8_COMPILER_ALLOCATION_BUILDER_H_

#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// A helper class to construct inline allocations on the simplified operator
// level. This keeps track of the effect chain for initial stores on a newly
// allocated object and also provides helpers for commonly allocated objects.
class AllocationBuilder final {
 public:
  AllocationBuilder(JSGraph* jsgraph, Node* effect, Node* control)
      : jsgraph_(jsgraph),
        allocation_(nullptr),
        effect_(effect),
        control_(control) {}

  // Primitive allocation of static size.
  void Allocate(int size, PretenureFlag pretenure = NOT_TENURED,
                Type* type = Type::Any()) {
    DCHECK_LE(size, kMaxRegularHeapObjectSize);
    effect_ = graph()->NewNode(
        common()->BeginRegion(RegionObservability::kNotObservable), effect_);
    allocation_ =
        graph()->NewNode(simplified()->Allocate(type, pretenure),
                         jsgraph()->Constant(size), effect_, control_);
    effect_ = allocation_;
  }

  // Primitive store into a field.
  void Store(const FieldAccess& access, Node* value) {
    effect_ = graph()->NewNode(simplified()->StoreField(access), allocation_,
                               value, effect_, control_);
  }

  // Primitive store into an element.
  void Store(ElementAccess const& access, Node* index, Node* value) {
    effect_ = graph()->NewNode(simplified()->StoreElement(access), allocation_,
                               index, value, effect_, control_);
  }

  // Compound allocation of a context.
  void AllocateContext(int length, Handle<Map> map) {
    DCHECK(map->instance_type() >= BLOCK_CONTEXT_TYPE &&
           map->instance_type() <= WITH_CONTEXT_TYPE);
    int size = FixedArray::SizeFor(length);
    Allocate(size, NOT_TENURED, Type::OtherInternal());
    Store(AccessBuilder::ForMap(), map);
    Store(AccessBuilder::ForFixedArrayLength(), jsgraph()->Constant(length));
  }

  // Compound allocation of a FixedArray.
  void AllocateArray(int length, Handle<Map> map,
                     PretenureFlag pretenure = NOT_TENURED) {
    DCHECK(map->instance_type() == FIXED_ARRAY_TYPE ||
           map->instance_type() == FIXED_DOUBLE_ARRAY_TYPE);
    int size = (map->instance_type() == FIXED_ARRAY_TYPE)
                   ? FixedArray::SizeFor(length)
                   : FixedDoubleArray::SizeFor(length);
    Allocate(size, pretenure, Type::OtherInternal());
    Store(AccessBuilder::ForMap(), map);
    Store(AccessBuilder::ForFixedArrayLength(), jsgraph()->Constant(length));
  }

  // Compound store of a constant into a field.
  void Store(const FieldAccess& access, Handle<Object> value) {
    Store(access, jsgraph()->Constant(value));
  }

  void FinishAndChange(Node* node) {
    NodeProperties::SetType(allocation_, NodeProperties::GetType(node));
    node->ReplaceInput(0, allocation_);
    node->ReplaceInput(1, effect_);
    node->TrimInputCount(2);
    NodeProperties::ChangeOp(node, common()->FinishRegion());
  }

  Node* Finish() {
    return graph()->NewNode(common()->FinishRegion(), allocation_, effect_);
  }

 protected:
  JSGraph* jsgraph() { return jsgraph_; }
  Graph* graph() { return jsgraph_->graph(); }
  CommonOperatorBuilder* common() { return jsgraph_->common(); }
  SimplifiedOperatorBuilder* simplified() { return jsgraph_->simplified(); }

 private:
  JSGraph* const jsgraph_;
  Node* allocation_;
  Node* effect_;
  Node* control_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ALLOCATION_BUILDER_H_
