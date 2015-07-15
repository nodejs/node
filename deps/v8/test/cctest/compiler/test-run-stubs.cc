// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/pipeline.h"
#include "src/parser.h"
#include "test/cctest/compiler/function-tester.h"

#if V8_TURBOFAN_TARGET

using namespace v8::internal;
using namespace v8::internal::compiler;


TEST(RunMathFloorStub) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();

  // Create code and an accompanying descriptor.
  MathFloorStub stub(isolate);
  Handle<Code> code = stub.GenerateCode();
  Zone* zone = scope.main_zone();

  CompilationInfo info(&stub, isolate, zone);
  CallDescriptor* descriptor = Linkage::ComputeIncoming(zone, &info);

  // Create a function to call the code using the descriptor.
  Graph graph(zone);
  CommonOperatorBuilder common(zone);
  JSOperatorBuilder javascript(zone);
  MachineOperatorBuilder machine(zone);
  JSGraph js(isolate, &graph, &common, &javascript, &machine);

  // FunctionTester (ab)uses a 2-argument function
  Node* start = graph.NewNode(common.Start(4));
  // Parameter 0 is the number to round
  Node* numberParam = graph.NewNode(common.Parameter(1), start);
  Unique<HeapObject> u = Unique<HeapObject>::CreateImmovable(code);
  Node* theCode = graph.NewNode(common.HeapConstant(u));
  Node* dummyContext = graph.NewNode(common.NumberConstant(0.0));
  Node* call = graph.NewNode(common.Call(descriptor), theCode,
                             js.UndefinedConstant(), js.UndefinedConstant(),
                             numberParam, dummyContext, start, start);
  Node* ret = graph.NewNode(common.Return(), call, call, start);
  Node* end = graph.NewNode(common.End(1), ret);
  graph.SetStart(start);
  graph.SetEnd(end);
  FunctionTester ft(&graph);

  Handle<Object> value = ft.Val(1.5);
  Handle<Object> result = ft.Call(value, value).ToHandleChecked();
  CHECK_EQ(1, Smi::cast(*result)->value());
}


TEST(RunStringLengthTFStub) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();

  // Create code and an accompanying descriptor.
  StringLengthTFStub stub(isolate);
  Handle<Code> code = stub.GenerateCode();
  CompilationInfo info(&stub, isolate, zone);
  CallDescriptor* descriptor = Linkage::ComputeIncoming(zone, &info);

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
  Unique<HeapObject> u = Unique<HeapObject>::CreateImmovable(code);
  Node* theCode = graph.NewNode(common.HeapConstant(u));
  Node* dummyContext = graph.NewNode(common.NumberConstant(0.0));
  Node* call =
      graph.NewNode(common.Call(descriptor), theCode, receiverParam, nameParam,
                    slotParam, vectorParam, dummyContext, start, start);
  Node* ret = graph.NewNode(common.Return(), call, call, start);
  Node* end = graph.NewNode(common.End(1), ret);
  graph.SetStart(start);
  graph.SetEnd(end);
  FunctionTester ft(&graph);

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


TEST(RunStringAddTFStub) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();

  // Create code and an accompanying descriptor.
  StringAddTFStub stub(isolate, STRING_ADD_CHECK_BOTH, NOT_TENURED);
  Handle<Code> code = stub.GenerateCode();
  CompilationInfo info(&stub, isolate, zone);
  CallDescriptor* descriptor = Linkage::ComputeIncoming(zone, &info);

  // Create a function to call the code using the descriptor.
  Graph graph(zone);
  CommonOperatorBuilder common(zone);
  // FunctionTester (ab)uses a 2-argument function
  Node* start = graph.NewNode(common.Start(4));
  // Parameter 0 is the receiver
  Node* leftParam = graph.NewNode(common.Parameter(1), start);
  Node* rightParam = graph.NewNode(common.Parameter(2), start);
  Unique<HeapObject> u = Unique<HeapObject>::CreateImmovable(code);
  Node* theCode = graph.NewNode(common.HeapConstant(u));
  Node* dummyContext = graph.NewNode(common.NumberConstant(0.0));
  Node* call = graph.NewNode(common.Call(descriptor), theCode, leftParam,
                             rightParam, dummyContext, start, start);
  Node* ret = graph.NewNode(common.Return(), call, call, start);
  Node* end = graph.NewNode(common.End(1), ret);
  graph.SetStart(start);
  graph.SetEnd(end);
  FunctionTester ft(&graph);

  // Actuall call through to the stub, verifying its result.
  Handle<String> leftArg = ft.Val("links");
  Handle<String> rightArg = ft.Val("rechts");
  Handle<Object> result = ft.Call(leftArg, rightArg).ToHandleChecked();
  CHECK(String::Equals(ft.Val("linksrechts"), Handle<String>::cast(result)));
}

#endif  // V8_TURBOFAN_TARGET
