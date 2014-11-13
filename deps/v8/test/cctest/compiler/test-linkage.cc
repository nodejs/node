// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/compiler.h"
#include "src/zone.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/schedule.h"
#include "test/cctest/cctest.h"

#if V8_TURBOFAN_TARGET

using namespace v8::internal;
using namespace v8::internal::compiler;

static Operator dummy_operator(IrOpcode::kParameter, Operator::kNoWrite,
                               "dummy", 0, 0, 0, 0, 0, 0);

// So we can get a real JS function.
static Handle<JSFunction> Compile(const char* source) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<String> source_code = isolate->factory()
                                   ->NewStringFromUtf8(CStrVector(source))
                                   .ToHandleChecked();
  Handle<SharedFunctionInfo> shared_function = Compiler::CompileScript(
      source_code, Handle<String>(), 0, 0, false,
      Handle<Context>(isolate->native_context()), NULL, NULL,
      v8::ScriptCompiler::kNoCompileOptions, NOT_NATIVES_CODE);
  return isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared_function, isolate->native_context());
}


TEST(TestLinkageCreate) {
  InitializedHandleScope handles;
  Handle<JSFunction> function = Compile("a + b");
  CompilationInfoWithZone info(function);
  Linkage linkage(info.zone(), &info);
}


TEST(TestLinkageJSFunctionIncoming) {
  InitializedHandleScope handles;

  const char* sources[] = {"(function() { })", "(function(a) { })",
                           "(function(a,b) { })", "(function(a,b,c) { })"};

  for (int i = 0; i < 3; i++) {
    i::HandleScope handles(CcTest::i_isolate());
    Handle<JSFunction> function = v8::Utils::OpenHandle(
        *v8::Handle<v8::Function>::Cast(CompileRun(sources[i])));
    CompilationInfoWithZone info(function);
    Linkage linkage(info.zone(), &info);

    CallDescriptor* descriptor = linkage.GetIncomingDescriptor();
    CHECK_NE(NULL, descriptor);

    CHECK_EQ(1 + i, descriptor->JSParameterCount());
    CHECK_EQ(1, descriptor->ReturnCount());
    CHECK_EQ(Operator::kNoProperties, descriptor->properties());
    CHECK_EQ(true, descriptor->IsJSFunctionCall());
  }
}


TEST(TestLinkageCodeStubIncoming) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  CompilationInfoWithZone info(static_cast<HydrogenCodeStub*>(NULL), isolate);
  Linkage linkage(info.zone(), &info);
  // TODO(titzer): test linkage creation with a bonafide code stub.
  // this just checks current behavior.
  CHECK_EQ(NULL, linkage.GetIncomingDescriptor());
}


TEST(TestLinkageJSCall) {
  HandleAndZoneScope handles;
  Handle<JSFunction> function = Compile("a + c");
  CompilationInfoWithZone info(function);
  Linkage linkage(info.zone(), &info);

  for (int i = 0; i < 32; i++) {
    CallDescriptor* descriptor =
        linkage.GetJSCallDescriptor(i, CallDescriptor::kNoFlags);
    CHECK_NE(NULL, descriptor);
    CHECK_EQ(i, descriptor->JSParameterCount());
    CHECK_EQ(1, descriptor->ReturnCount());
    CHECK_EQ(Operator::kNoProperties, descriptor->properties());
    CHECK_EQ(true, descriptor->IsJSFunctionCall());
  }
}


TEST(TestLinkageRuntimeCall) {
  // TODO(titzer): test linkage creation for outgoing runtime calls.
}


TEST(TestLinkageStubCall) {
  // TODO(titzer): test linkage creation for outgoing stub calls.
}


#endif  // V8_TURBOFAN_TARGET
