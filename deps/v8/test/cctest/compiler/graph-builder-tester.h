// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_
#define V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-builder.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/simplified-operator.h"
#include "test/cctest/compiler/call-tester.h"
#include "test/cctest/compiler/simplified-graph-builder.h"

namespace v8 {
namespace internal {
namespace compiler {

class GraphAndBuilders {
 public:
  explicit GraphAndBuilders(Zone* zone)
      : main_graph_(new (zone) Graph(zone)),
        main_common_(zone),
        main_machine_(zone),
        main_simplified_(zone) {}

 protected:
  // Prefixed with main_ to avoid naming conflicts.
  Graph* main_graph_;
  CommonOperatorBuilder main_common_;
  MachineOperatorBuilder main_machine_;
  SimplifiedOperatorBuilder main_simplified_;
};


template <typename ReturnType>
class GraphBuilderTester
    : public HandleAndZoneScope,
      private GraphAndBuilders,
      public CallHelper,
      public SimplifiedGraphBuilder,
      public CallHelper2<ReturnType, GraphBuilderTester<ReturnType> > {
 public:
  explicit GraphBuilderTester(MachineType p0 = kMachNone,
                              MachineType p1 = kMachNone,
                              MachineType p2 = kMachNone,
                              MachineType p3 = kMachNone,
                              MachineType p4 = kMachNone)
      : GraphAndBuilders(main_zone()),
        CallHelper(
            main_isolate(),
            MakeMachineSignature(
                main_zone(), ReturnValueTraits<ReturnType>::Representation(),
                p0, p1, p2, p3, p4)),
        SimplifiedGraphBuilder(main_isolate(), main_graph_, &main_common_,
                               &main_machine_, &main_simplified_),
        parameters_(main_zone()->template NewArray<Node*>(parameter_count())) {
    Begin(static_cast<int>(parameter_count()));
    InitParameters();
  }
  virtual ~GraphBuilderTester() {}

  void GenerateCode() { Generate(); }
  Node* Parameter(size_t index) {
    DCHECK(index < parameter_count());
    return parameters_[index];
  }

  Factory* factory() const { return isolate()->factory(); }

 protected:
  virtual byte* Generate() {
    if (!Pipeline::SupportedBackend()) return NULL;
    if (code_.is_null()) {
      Zone* zone = graph()->zone();
      CallDescriptor* desc =
          Linkage::GetSimplifiedCDescriptor(zone, machine_sig_);
      code_ = Pipeline::GenerateCodeForTesting(main_isolate(), desc, graph());
    }
    return code_.ToHandleChecked()->entry();
  }

  void InitParameters() {
    int param_count = static_cast<int>(parameter_count());
    for (int i = 0; i < param_count; ++i) {
      parameters_[i] = this->NewNode(common()->Parameter(i), graph()->start());
    }
  }

  size_t parameter_count() const { return machine_sig_->parameter_count(); }

 private:
  Node** parameters_;
  MaybeHandle<Code> code_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_
