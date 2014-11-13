// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_
#define V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-builder.h"
#include "src/compiler/machine-operator.h"
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
  virtual Node* MakeNode(const Operator* op, int value_input_count,
                         Node** value_inputs, bool incomplete) FINAL {
    return graph()->NewNode(op, value_input_count, value_inputs, incomplete);
  }
};


class MachineCallHelper : public CallHelper {
 public:
  MachineCallHelper(Zone* zone, MachineSignature* machine_sig);

  Node* Parameter(size_t index);

  void GenerateCode() { Generate(); }

 protected:
  virtual byte* Generate();
  void InitParameters(GraphBuilder* builder, CommonOperatorBuilder* common);

 protected:
  size_t parameter_count() const { return machine_sig_->parameter_count(); }

 private:
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
  explicit GraphBuilderTester(MachineType p0 = kMachNone,
                              MachineType p1 = kMachNone,
                              MachineType p2 = kMachNone,
                              MachineType p3 = kMachNone,
                              MachineType p4 = kMachNone)
      : GraphAndBuilders(main_zone()),
        MachineCallHelper(
            main_zone(),
            MakeMachineSignature(
                main_zone(), ReturnValueTraits<ReturnType>::Representation(),
                p0, p1, p2, p3, p4)),
        SimplifiedGraphBuilder(main_graph_, &main_common_, &main_machine_,
                               &main_simplified_) {
    Begin(static_cast<int>(parameter_count()));
    InitParameters(this, &main_common_);
  }
  virtual ~GraphBuilderTester() {}

  Factory* factory() const { return isolate()->factory(); }
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_
