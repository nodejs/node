// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_SIMPLIFIED_GRAPH_BUILDER_H_
#define V8_CCTEST_COMPILER_SIMPLIFIED_GRAPH_BUILDER_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-builder.h"
#include "src/compiler/machine-node-factory.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/simplified-node-factory.h"
#include "src/compiler/simplified-operator.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/call-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

class SimplifiedGraphBuilder
    : public StructuredGraphBuilder,
      public MachineNodeFactory<SimplifiedGraphBuilder>,
      public SimplifiedNodeFactory<SimplifiedGraphBuilder> {
 public:
  SimplifiedGraphBuilder(Graph* graph, CommonOperatorBuilder* common,
                         MachineOperatorBuilder* machine,
                         SimplifiedOperatorBuilder* simplified);
  virtual ~SimplifiedGraphBuilder() {}

  class Environment : public StructuredGraphBuilder::Environment {
   public:
    Environment(SimplifiedGraphBuilder* builder, Node* control_dependency);

    // TODO(dcarney): encode somehow and merge into StructuredGraphBuilder.
    // SSA renaming operations.
    Node* Top();
    void Push(Node* node);
    Node* Pop();
    void Poke(size_t depth, Node* node);
    Node* Peek(size_t depth);
  };

  Isolate* isolate() const { return zone()->isolate(); }
  Zone* zone() const { return StructuredGraphBuilder::zone(); }
  CommonOperatorBuilder* common() const {
    return StructuredGraphBuilder::common();
  }
  MachineOperatorBuilder* machine() const { return machine_; }
  SimplifiedOperatorBuilder* simplified() const { return simplified_; }
  Environment* environment() {
    return reinterpret_cast<Environment*>(
        StructuredGraphBuilder::environment());
  }

  // Initialize graph and builder.
  void Begin(int num_parameters);

  void Return(Node* value);

  // Close the graph.
  void End();

 private:
  MachineOperatorBuilder* machine_;
  SimplifiedOperatorBuilder* simplified_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_SIMPLIFIED_GRAPH_BUILDER_H_
