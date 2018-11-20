// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_BUILDER_H_
#define V8_COMPILER_GRAPH_BUILDER_H_

#include "src/allocation.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/unique.h"

namespace v8 {
namespace internal {
namespace compiler {

// A common base class for anything that creates nodes in a graph.
class GraphBuilder {
 public:
  GraphBuilder(Isolate* isolate, Graph* graph)
      : isolate_(isolate), graph_(graph) {}
  virtual ~GraphBuilder() {}

  Node* NewNode(const Operator* op, bool incomplete = false) {
    return MakeNode(op, 0, static_cast<Node**>(NULL), incomplete);
  }

  Node* NewNode(const Operator* op, Node* n1) {
    return MakeNode(op, 1, &n1, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2) {
    Node* buffer[] = {n1, n2};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3) {
    Node* buffer[] = {n1, n2, n3};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4) {
    Node* buffer[] = {n1, n2, n3, n4};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5) {
    Node* buffer[] = {n1, n2, n3, n4, n5};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5, Node* n6) {
    Node* nodes[] = {n1, n2, n3, n4, n5, n6};
    return MakeNode(op, arraysize(nodes), nodes, false);
  }

  Node* NewNode(const Operator* op, int value_input_count, Node** value_inputs,
                bool incomplete = false) {
    return MakeNode(op, value_input_count, value_inputs, incomplete);
  }

  Isolate* isolate() const { return isolate_; }
  Graph* graph() const { return graph_; }

 protected:
  // Base implementation used by all factory methods.
  virtual Node* MakeNode(const Operator* op, int value_input_count,
                         Node** value_inputs, bool incomplete) = 0;

 private:
  Isolate* isolate_;
  Graph* graph_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_BUILDER_H__
