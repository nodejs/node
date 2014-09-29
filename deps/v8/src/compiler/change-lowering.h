// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CHANGE_LOWERING_H_
#define V8_COMPILER_CHANGE_LOWERING_H_

#include "include/v8.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/machine-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonNodeCache;
class Linkage;

class ChangeLoweringBase : public Reducer {
 public:
  ChangeLoweringBase(Graph* graph, Linkage* linkage, CommonNodeCache* cache);
  virtual ~ChangeLoweringBase();

 protected:
  Node* ExternalConstant(ExternalReference reference);
  Node* HeapConstant(PrintableUnique<HeapObject> value);
  Node* ImmovableHeapConstant(Handle<HeapObject> value);
  Node* Int32Constant(int32_t value);
  Node* NumberConstant(double value);
  Node* CEntryStubConstant();
  Node* TrueConstant();
  Node* FalseConstant();

  Reduction ChangeBitToBool(Node* val, Node* control);

  Graph* graph() const { return graph_; }
  Isolate* isolate() const { return isolate_; }
  Linkage* linkage() const { return linkage_; }
  CommonNodeCache* cache() const { return cache_; }
  CommonOperatorBuilder* common() { return &common_; }
  MachineOperatorBuilder* machine() { return &machine_; }

 private:
  Graph* graph_;
  Isolate* isolate_;
  Linkage* linkage_;
  CommonNodeCache* cache_;
  CommonOperatorBuilder common_;
  MachineOperatorBuilder machine_;

  SetOncePointer<Node> c_entry_stub_constant_;
  SetOncePointer<Node> true_constant_;
  SetOncePointer<Node> false_constant_;
};


template <size_t kPointerSize = kApiPointerSize>
class ChangeLowering V8_FINAL : public ChangeLoweringBase {
 public:
  ChangeLowering(Graph* graph, Linkage* linkage);
  ChangeLowering(Graph* graph, Linkage* linkage, CommonNodeCache* cache)
      : ChangeLoweringBase(graph, linkage, cache) {}
  virtual ~ChangeLowering() {}

  virtual Reduction Reduce(Node* node) V8_OVERRIDE;

 private:
  Reduction ChangeBoolToBit(Node* val);
  Reduction ChangeInt32ToTagged(Node* val, Node* effect, Node* control);
  Reduction ChangeTaggedToFloat64(Node* val, Node* effect, Node* control);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CHANGE_LOWERING_H_
