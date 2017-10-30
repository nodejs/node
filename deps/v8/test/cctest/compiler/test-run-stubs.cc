// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"
#include "src/callable.h"
#include "src/code-stubs.h"
#include "src/compilation-info.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/pipeline.h"
#include "src/objects-inl.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

class StubTester {
 public:
  StubTester(Isolate* isolate, Zone* zone, CodeStub* stub)
      : zone_(zone),
        info_(ArrayVector("test"), isolate, zone,
              Code::ComputeFlags(Code::HANDLER)),
        interface_descriptor_(stub->GetCallInterfaceDescriptor()),
        descriptor_(Linkage::GetStubCallDescriptor(
            isolate, zone, interface_descriptor_,
            stub->GetStackParameterCount(), CallDescriptor::kNoFlags,
            Operator::kNoProperties)),
        graph_(zone_),
        common_(zone_),
        tester_(InitializeFunctionTester(stub->GetCode()),
                GetParameterCountWithContext()) {}

  StubTester(Isolate* isolate, Zone* zone, Builtins::Name name)
      : zone_(zone),
        info_(ArrayVector("test"), isolate, zone,
              Code::ComputeFlags(Code::HANDLER)),
        interface_descriptor_(
            Builtins::CallableFor(isolate, name).descriptor()),
        descriptor_(Linkage::GetStubCallDescriptor(
            isolate, zone, interface_descriptor_,
            interface_descriptor_.GetStackParameterCount(),
            CallDescriptor::kNoFlags, Operator::kNoProperties)),
        graph_(zone_),
        common_(zone_),
        tester_(InitializeFunctionTester(
                    Handle<Code>(isolate->builtins()->builtin(name))),
                GetParameterCountWithContext()) {}

  template <typename... Args>
  Handle<Object> Call(Args... args) {
    DCHECK_EQ(interface_descriptor_.GetParameterCount(), sizeof...(args));
    MaybeHandle<Object> result =
        tester_.Call(args..., Handle<HeapObject>(tester_.function->context()))
            .ToHandleChecked();
    return result.ToHandleChecked();
  }

  FunctionTester& ft() { return tester_; }

 private:
  Graph* InitializeFunctionTester(Handle<Code> stub) {
    // Add target, effect and control.
    int node_count = GetParameterCountWithContext() + 3;
    // Add extra inputs for the JSFunction parameter and the receiver (which for
    // the tester is always undefined) to the start node.
    Node* start =
        graph_.NewNode(common_.Start(GetParameterCountWithContext() + 2));
    Node** node_array = zone_->NewArray<Node*>(node_count);
    node_array[0] = graph_.NewNode(common_.HeapConstant(stub));
    for (int i = 0; i < GetParameterCountWithContext(); ++i) {
      CHECK(IsAnyTagged(descriptor_->GetParameterType(i).representation()));
      node_array[i + 1] = graph_.NewNode(common_.Parameter(i + 1), start);
    }
    node_array[node_count - 2] = start;
    node_array[node_count - 1] = start;
    Node* call =
        graph_.NewNode(common_.Call(descriptor_), node_count, &node_array[0]);

    Node* zero = graph_.NewNode(common_.Int32Constant(0));
    Node* ret = graph_.NewNode(common_.Return(), zero, call, call, start);
    Node* end = graph_.NewNode(common_.End(1), ret);
    graph_.SetStart(start);
    graph_.SetEnd(end);
    return &graph_;
  }

  int GetParameterCountWithContext() {
    return interface_descriptor_.GetParameterCount() + 1;
  }

  Zone* zone_;
  CompilationInfo info_;
  CallInterfaceDescriptor interface_descriptor_;
  CallDescriptor* descriptor_;
  Graph graph_;
  CommonOperatorBuilder common_;
  FunctionTester tester_;
};

TEST(RunStringLengthStub) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();

  StringLengthStub stub(isolate);
  StubTester tester(isolate, zone, &stub);

  // Actuall call through to the stub, verifying its result.
  const char* testString = "Und das Lamm schrie HURZ!";
  Handle<Object> receiverArg =
      Object::ToObject(isolate, tester.ft().Val(testString)).ToHandleChecked();
  Handle<Object> nameArg = tester.ft().Val("length");
  Handle<Object> slot = tester.ft().Val(0.0);
  Handle<Object> vector = tester.ft().Val(0.0);
  Handle<Object> result = tester.Call(receiverArg, nameArg, slot, vector);
  CHECK_EQ(static_cast<int>(strlen(testString)), Smi::ToInt(*result));
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
