// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_
#define V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-builder.h"
#include "src/compiler/machine-node-factory.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/simplified-node-factory.h"
#include "src/compiler/simplified-operator.h"
#include "test/cctest/compiler/call-tester.h"
#include "test/cctest/compiler/simplified-graph-builder.h"

namespace v8 {
namespace internal {
namespace compiler {

// A class that just passes node creation on to the Graph.
class DirectGraphBuilder : public GraphBuilder {
 public:
  explicit DirectGraphBuilder(Graph* graph) : GraphBuilder(graph) {}
  virtual ~DirectGraphBuilder() {}

 protected:
  virtual Node* MakeNode(Operator* op, int value_input_count,
                         Node** value_inputs) {
    return graph()->NewNode(op, value_input_count, value_inputs);
  }
};


class MachineCallHelper : public CallHelper {
 public:
  MachineCallHelper(Zone* zone, MachineCallDescriptorBuilder* builder);

  Node* Parameter(int offset);

  void GenerateCode() { Generate(); }

 protected:
  virtual byte* Generate();
  virtual void VerifyParameters(int parameter_count, MachineType* parameters);
  void InitParameters(GraphBuilder* builder, CommonOperatorBuilder* common);

 protected:
  int parameter_count() const {
    return call_descriptor_builder_->parameter_count();
  }

 private:
  MachineCallDescriptorBuilder* call_descriptor_builder_;
  Node** parameters_;
  // TODO(dcarney): shouldn't need graph stored.
  Graph* graph_;
  MaybeHandle<Code> code_;
};


class GraphAndBuilders {
 public:
  explicit GraphAndBuilders(Zone* zone)
      : main_graph_(new (zone) Graph(zone)),
        main_common_(zone),
        main_machine_(zone),
        main_simplified_(zone) {}

 protected:
  // Prefixed with main_ to avoid naiming conflicts.
  Graph* main_graph_;
  CommonOperatorBuilder main_common_;
  MachineOperatorBuilder main_machine_;
  SimplifiedOperatorBuilder main_simplified_;
};


template <typename ReturnType>
class GraphBuilderTester
    : public HandleAndZoneScope,
      private GraphAndBuilders,
      public MachineCallHelper,
      public SimplifiedGraphBuilder,
      public CallHelper2<ReturnType, GraphBuilderTester<ReturnType> > {
 public:
  explicit GraphBuilderTester(MachineType p0 = kMachineLast,
                              MachineType p1 = kMachineLast,
                              MachineType p2 = kMachineLast,
                              MachineType p3 = kMachineLast,
                              MachineType p4 = kMachineLast)
      : GraphAndBuilders(main_zone()),
        MachineCallHelper(
            main_zone(),
            ToCallDescriptorBuilder(
                main_zone(), ReturnValueTraits<ReturnType>::Representation(),
                p0, p1, p2, p3, p4)),
        SimplifiedGraphBuilder(main_graph_, &main_common_, &main_machine_,
                               &main_simplified_) {
    Begin(parameter_count());
    InitParameters(this, &main_common_);
  }
  virtual ~GraphBuilderTester() {}

  Factory* factory() const { return isolate()->factory(); }
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_
