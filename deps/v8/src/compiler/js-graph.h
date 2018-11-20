// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_GRAPH_H_
#define V8_COMPILER_JS_GRAPH_H_

#include "src/compiler/common-node-cache.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

class Typer;

// Implements a facade on a Graph, enhancing the graph with JS-specific
// notions, including a builder for for JS* operators, canonicalized global
// constants, and various helper methods.
class JSGraph : public ZoneObject {
 public:
  JSGraph(Isolate* isolate, Graph* graph, CommonOperatorBuilder* common,
          JSOperatorBuilder* javascript, MachineOperatorBuilder* machine)
      : isolate_(isolate),
        graph_(graph),
        common_(common),
        javascript_(javascript),
        machine_(machine),
        cache_(zone()) {}

  // Canonicalized global constants.
  Node* CEntryStubConstant(int result_size);
  Node* UndefinedConstant();
  Node* TheHoleConstant();
  Node* TrueConstant();
  Node* FalseConstant();
  Node* NullConstant();
  Node* ZeroConstant();
  Node* OneConstant();
  Node* NaNConstant();

  // Creates a HeapConstant node, possibly canonicalized, without inspecting the
  // object.
  Node* HeapConstant(Unique<HeapObject> value);

  // Creates a HeapConstant node, possibly canonicalized, and may access the
  // heap to inspect the object.
  Node* HeapConstant(Handle<HeapObject> value);

  // Creates a Constant node of the appropriate type for the given object.
  // Accesses the heap to inspect the object and determine whether one of the
  // canonicalized globals or a number constant should be returned.
  Node* Constant(Handle<Object> value);

  // Creates a NumberConstant node, usually canonicalized.
  Node* Constant(double value);

  // Creates a NumberConstant node, usually canonicalized.
  Node* Constant(int32_t value);

  // Creates a Int32Constant node, usually canonicalized.
  Node* Int32Constant(int32_t value);
  Node* Uint32Constant(uint32_t value) {
    return Int32Constant(bit_cast<int32_t>(value));
  }

  // Creates a HeapConstant node for either true or false.
  Node* BooleanConstant(bool is_true) {
    return is_true ? TrueConstant() : FalseConstant();
  }

  // Creates a Int64Constant node, usually canonicalized.
  Node* Int64Constant(int64_t value);
  Node* Uint64Constant(uint64_t value) {
    return Int64Constant(bit_cast<int64_t>(value));
  }

  // Creates a Int32Constant/Int64Constant node, depending on the word size of
  // the target machine.
  // TODO(turbofan): Code using Int32Constant/Int64Constant to store pointer
  // constants is probably not serializable.
  Node* IntPtrConstant(intptr_t value) {
    return machine()->Is32() ? Int32Constant(static_cast<int32_t>(value))
                             : Int64Constant(static_cast<int64_t>(value));
  }
  template <typename T>
  Node* PointerConstant(T* value) {
    return IntPtrConstant(bit_cast<intptr_t>(value));
  }

  // Creates a Float32Constant node, usually canonicalized.
  Node* Float32Constant(float value);

  // Creates a Float64Constant node, usually canonicalized.
  Node* Float64Constant(double value);

  // Creates an ExternalConstant node, usually canonicalized.
  Node* ExternalConstant(ExternalReference ref);

  Node* SmiConstant(int32_t immediate) {
    DCHECK(Smi::IsValid(immediate));
    return Constant(immediate);
  }

  // Creates a dummy Constant node, used to satisfy calling conventions of
  // stubs and runtime functions that do not require a context.
  Node* NoContextConstant() { return ZeroConstant(); }

  // Creates an empty frame states for cases where we know that a function
  // cannot deopt.
  Node* EmptyFrameState();

  // Create a control node that serves as control dependency for dead nodes.
  Node* DeadControl();

  JSOperatorBuilder* javascript() const { return javascript_; }
  CommonOperatorBuilder* common() const { return common_; }
  MachineOperatorBuilder* machine() const { return machine_; }
  Graph* graph() const { return graph_; }
  Zone* zone() const { return graph()->zone(); }
  Isolate* isolate() const { return isolate_; }
  Factory* factory() const { return isolate()->factory(); }

  void GetCachedNodes(NodeVector* nodes);

 private:
  Isolate* isolate_;
  Graph* graph_;
  CommonOperatorBuilder* common_;
  JSOperatorBuilder* javascript_;
  MachineOperatorBuilder* machine_;

  // TODO(titzer): make this into a simple array.
  SetOncePointer<Node> c_entry_stub_constant_;
  SetOncePointer<Node> undefined_constant_;
  SetOncePointer<Node> the_hole_constant_;
  SetOncePointer<Node> true_constant_;
  SetOncePointer<Node> false_constant_;
  SetOncePointer<Node> null_constant_;
  SetOncePointer<Node> zero_constant_;
  SetOncePointer<Node> one_constant_;
  SetOncePointer<Node> nan_constant_;
  SetOncePointer<Node> empty_frame_state_;
  SetOncePointer<Node> dead_control_;

  CommonNodeCache cache_;

  Node* ImmovableHeapConstant(Handle<HeapObject> value);
  Node* NumberConstant(double value);

  DISALLOW_COPY_AND_ASSIGN(JSGraph);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif
