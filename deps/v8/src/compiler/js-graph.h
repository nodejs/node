// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_GRAPH_H_
#define V8_COMPILER_JS_GRAPH_H_

#include "src/compiler/common-node-cache.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-operator.h"
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
  JSGraph(Graph* graph, CommonOperatorBuilder* common, Typer* typer)
      : graph_(graph),
        common_(common),
        javascript_(zone()),
        typer_(typer),
        cache_(zone()) {}

  // Canonicalized global constants.
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
  Node* HeapConstant(PrintableUnique<Object> value);

  // Creates a HeapConstant node, possibly canonicalized, and may access the
  // heap to inspect the object.
  Node* HeapConstant(Handle<Object> value);

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

  // Creates a Float64Constant node, usually canonicalized.
  Node* Float64Constant(double value);

  // Creates an ExternalConstant node, usually canonicalized.
  Node* ExternalConstant(ExternalReference ref);

  Node* SmiConstant(int32_t immediate) {
    DCHECK(Smi::IsValid(immediate));
    return Constant(immediate);
  }

  JSOperatorBuilder* javascript() { return &javascript_; }
  CommonOperatorBuilder* common() { return common_; }
  Graph* graph() { return graph_; }
  Zone* zone() { return graph()->zone(); }

 private:
  Graph* graph_;
  CommonOperatorBuilder* common_;
  JSOperatorBuilder javascript_;
  Typer* typer_;

  SetOncePointer<Node> undefined_constant_;
  SetOncePointer<Node> the_hole_constant_;
  SetOncePointer<Node> true_constant_;
  SetOncePointer<Node> false_constant_;
  SetOncePointer<Node> null_constant_;
  SetOncePointer<Node> zero_constant_;
  SetOncePointer<Node> one_constant_;
  SetOncePointer<Node> nan_constant_;

  CommonNodeCache cache_;

  Node* ImmovableHeapConstant(Handle<Object> value);
  Node* NumberConstant(double value);
  Node* NewNode(Operator* op);

  Factory* factory() { return zone()->isolate()->factory(); }
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif
