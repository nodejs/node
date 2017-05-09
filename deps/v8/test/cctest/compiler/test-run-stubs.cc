// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"
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


TEST(RunStringLengthStub) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();

  // Create code and an accompanying descriptor.
  StringLengthStub stub(isolate);
  Handle<Code> code = stub.GenerateCode();
  CompilationInfo info(ArrayVector("test"), isolate, zone,
                       Code::ComputeFlags(Code::HANDLER));
  CallInterfaceDescriptor interface_descriptor =
      stub.GetCallInterfaceDescriptor();
  CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
      isolate, zone, interface_descriptor, stub.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties);

  // Create a function to call the code using the descriptor.
  Graph graph(zone);
  CommonOperatorBuilder common(zone);
  // FunctionTester (ab)uses a 4-argument function
  Node* start = graph.NewNode(common.Start(6));
  // Parameter 0 is the receiver
  Node* receiverParam = graph.NewNode(common.Parameter(1), start);
  Node* nameParam = graph.NewNode(common.Parameter(2), start);
  Node* slotParam = graph.NewNode(common.Parameter(3), start);
  Node* vectorParam = graph.NewNode(common.Parameter(4), start);
  Node* theCode = graph.NewNode(common.HeapConstant(code));
  Node* dummyContext = graph.NewNode(common.NumberConstant(0.0));
  Node* zero = graph.NewNode(common.Int32Constant(0));
  Node* call =
      graph.NewNode(common.Call(descriptor), theCode, receiverParam, nameParam,
                    slotParam, vectorParam, dummyContext, start, start);
  Node* ret = graph.NewNode(common.Return(), zero, call, call, start);
  Node* end = graph.NewNode(common.End(1), ret);
  graph.SetStart(start);
  graph.SetEnd(end);
  FunctionTester ft(&graph, 4);

  // Actuall call through to the stub, verifying its result.
  const char* testString = "Und das Lamm schrie HURZ!";
  Handle<JSReceiver> receiverArg =
      Object::ToObject(isolate, ft.Val(testString)).ToHandleChecked();
  Handle<String> nameArg = ft.Val("length");
  Handle<Object> slot = ft.Val(0.0);
  Handle<Object> vector = ft.Val(0.0);
  Handle<Object> result =
      ft.Call(receiverArg, nameArg, slot, vector).ToHandleChecked();
  CHECK_EQ(static_cast<int>(strlen(testString)), Smi::cast(*result)->value());
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
