// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/parser.h"
#include "test/cctest/compiler/function-tester.h"

#if V8_TURBOFAN_TARGET

using namespace v8::internal;
using namespace v8::internal::compiler;


static Handle<JSFunction> GetFunction(Isolate* isolate, const char* name) {
  v8::ExtensionConfiguration no_extensions;
  Handle<Context> ctx = isolate->bootstrapper()->CreateEnvironment(
      MaybeHandle<JSGlobalProxy>(), v8::Handle<v8::ObjectTemplate>(),
      &no_extensions);
  Handle<JSBuiltinsObject> builtins = handle(ctx->builtins());
  MaybeHandle<Object> fun = Object::GetProperty(isolate, builtins, name);
  Handle<JSFunction> function = Handle<JSFunction>::cast(fun.ToHandleChecked());
  // Just to make sure nobody calls this...
  function->set_code(isolate->builtins()->builtin(Builtins::kIllegal));
  return function;
}


class StringLengthStubTF : public CodeStub {
 public:
  explicit StringLengthStubTF(Isolate* isolate) : CodeStub(isolate) {}

  StringLengthStubTF(uint32_t key, Isolate* isolate) : CodeStub(key, isolate) {}

  CallInterfaceDescriptor GetCallInterfaceDescriptor() override {
    return LoadDescriptor(isolate());
  };

  Handle<Code> GenerateCode() override {
    Zone zone;
    // Build a "hybrid" CompilationInfo for a JSFunction/CodeStub pair.
    ParseInfo parse_info(&zone, GetFunction(isolate(), "STRING_LENGTH_STUB"));
    CompilationInfo info(&parse_info);
    info.SetStub(this);
    // Run a "mini pipeline", extracted from compiler.cc.
    CHECK(Parser::ParseStatic(info.parse_info()));
    CHECK(Compiler::Analyze(info.parse_info()));
    return Pipeline(&info).GenerateCode();
  }

  Major MajorKey() const override { return StringLength; };
  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  InlineCacheState GetICState() const override { return MONOMORPHIC; }
  ExtraICState GetExtraICState() const override { return Code::LOAD_IC; }
  Code::StubType GetStubType() const override { return Code::FAST; }

 private:
  DISALLOW_COPY_AND_ASSIGN(StringLengthStubTF);
};


TEST(RunStringLengthStubTF) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();

  // Create code and an accompanying descriptor.
  StringLengthStubTF stub(isolate);
  Handle<Code> code = stub.GenerateCode();
  CompilationInfo info(&stub, isolate, zone);
  CallDescriptor* descriptor = Linkage::ComputeIncoming(zone, &info);

  // Create a function to call the code using the descriptor.
  Graph graph(zone);
  CommonOperatorBuilder common(zone);
  // FunctionTester (ab)uses a 2-argument function
  Node* start = graph.NewNode(common.Start(2));
  // Parameter 0 is the receiver
  Node* receiverParam = graph.NewNode(common.Parameter(1), start);
  Node* nameParam = graph.NewNode(common.Parameter(2), start);
  Unique<HeapObject> u = Unique<HeapObject>::CreateImmovable(code);
  Node* theCode = graph.NewNode(common.HeapConstant(u));
  Node* dummyContext = graph.NewNode(common.NumberConstant(0.0));
  Node* call = graph.NewNode(common.Call(descriptor), theCode, receiverParam,
                             nameParam, dummyContext, start, start);
  Node* ret = graph.NewNode(common.Return(), call, call, start);
  Node* end = graph.NewNode(common.End(), ret);
  graph.SetStart(start);
  graph.SetEnd(end);
  FunctionTester ft(&graph);

  // Actuall call through to the stub, verifying its result.
  const char* testString = "Und das Lamm schrie HURZ!";
  Handle<JSReceiver> receiverArg =
      Object::ToObject(isolate, ft.Val(testString)).ToHandleChecked();
  Handle<String> nameArg = ft.Val("length");
  Handle<Object> result = ft.Call(receiverArg, nameArg).ToHandleChecked();
  CHECK_EQ(static_cast<int>(strlen(testString)), Smi::cast(*result)->value());
}

#endif  // V8_TURBOFAN_TARGET
