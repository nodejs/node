// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/schedule.h"
#include "src/parsing/parse-info.h"
#include "src/zone/zone.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

static Operator dummy_operator(IrOpcode::kParameter, Operator::kNoWrite,
                               "dummy", 0, 0, 0, 0, 0, 0);

// So we can get a real JS function.
static Handle<JSFunction> Compile(const char* source) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<String> source_code = isolate->factory()
                                   ->NewStringFromUtf8(CStrVector(source))
                                   .ToHandleChecked();
  Handle<SharedFunctionInfo> shared = Compiler::GetSharedFunctionInfoForScript(
      source_code, Handle<String>(), 0, 0, v8::ScriptOriginOptions(),
      Handle<Object>(), Handle<Context>(isolate->native_context()), NULL, NULL,
      v8::ScriptCompiler::kNoCompileOptions, NOT_NATIVES_CODE, false);
  return isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared, isolate->native_context());
}


TEST(TestLinkageCreate) {
  HandleAndZoneScope handles;
  Handle<JSFunction> function = Compile("a + b");
  ParseInfo parse_info(handles.main_zone(), function);
  CompilationInfo info(&parse_info, function);
  CallDescriptor* descriptor = Linkage::ComputeIncoming(info.zone(), &info);
  CHECK(descriptor);
}


TEST(TestLinkageJSFunctionIncoming) {
  const char* sources[] = {"(function() { })", "(function(a) { })",
                           "(function(a,b) { })", "(function(a,b,c) { })"};

  for (int i = 0; i < 3; i++) {
    HandleAndZoneScope handles;
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(v8::Utils::OpenHandle(
            *v8::Local<v8::Function>::Cast(CompileRun(sources[i]))));
    ParseInfo parse_info(handles.main_zone(), function);
    CompilationInfo info(&parse_info, function);
    CallDescriptor* descriptor = Linkage::ComputeIncoming(info.zone(), &info);
    CHECK(descriptor);

    CHECK_EQ(1 + i, static_cast<int>(descriptor->JSParameterCount()));
    CHECK_EQ(1, static_cast<int>(descriptor->ReturnCount()));
    CHECK_EQ(Operator::kNoProperties, descriptor->properties());
    CHECK_EQ(true, descriptor->IsJSFunctionCall());
  }
}


TEST(TestLinkageJSCall) {
  HandleAndZoneScope handles;
  Handle<JSFunction> function = Compile("a + c");
  ParseInfo parse_info(handles.main_zone(), function);
  CompilationInfo info(&parse_info, function);

  for (int i = 0; i < 32; i++) {
    CallDescriptor* descriptor = Linkage::GetJSCallDescriptor(
        info.zone(), false, i, CallDescriptor::kNoFlags);
    CHECK(descriptor);
    CHECK_EQ(i, static_cast<int>(descriptor->JSParameterCount()));
    CHECK_EQ(1, static_cast<int>(descriptor->ReturnCount()));
    CHECK_EQ(Operator::kNoProperties, descriptor->properties());
    CHECK_EQ(true, descriptor->IsJSFunctionCall());
  }
}


TEST(TestLinkageRuntimeCall) {
  // TODO(titzer): test linkage creation for outgoing runtime calls.
}


TEST(TestLinkageStubCall) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  Zone zone(isolate->allocator());
  Callable callable = CodeFactory::ToNumber(isolate);
  CompilationInfo info(ArrayVector("test"), isolate, &zone,
                       Code::ComputeFlags(Code::STUB));
  CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
      isolate, &zone, callable.descriptor(), 0, CallDescriptor::kNoFlags,
      Operator::kNoProperties);
  CHECK(descriptor);
  CHECK_EQ(0, static_cast<int>(descriptor->StackParameterCount()));
  CHECK_EQ(1, static_cast<int>(descriptor->ReturnCount()));
  CHECK_EQ(Operator::kNoProperties, descriptor->properties());
  CHECK_EQ(false, descriptor->IsJSFunctionCall());
  // TODO(titzer): test linkage creation for outgoing stub calls.
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
