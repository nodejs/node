// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/linkage.h"

#include <array>
#include <string_view>

#include "include/v8-function.h"
#include "src/api/api-inl.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/compiler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/codegen/script-details.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/schedule.h"
#include "src/compiler/turbofan-graph.h"
#include "src/objects/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/zone/zone.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::compiler {

namespace {
v8::Local<v8::Value> CompileRun(v8::Isolate* isolate, const char* source) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Script> script =
      v8::Script::Compile(
          context, v8::String::NewFromUtf8(isolate, source).ToLocalChecked())
          .ToLocalChecked();
  return script->Run(context).ToLocalChecked();
}

constexpr auto make_test_sources() {
  return std::array{"(function() { })", "(function(a) { })",
                    "(function(a,b) { })", "(function(a,b,c) { })"};
}
}  // namespace

using LinkageTest = TestWithNativeContextAndZone;

static Operator dummy_operator(IrOpcode::kParameter, Operator::kNoWrite,
                               "dummy", 0, 0, 0, 0, 0, 0);

// So we can get a real JS function.
static Handle<JSFunction> Compile(Isolate* isolate, std::string_view source) {
  Handle<String> source_code = isolate->factory()
                                   ->NewStringFromUtf8(base::Vector<const char>(
                                       source.data(), source.length()))
                                   .ToHandleChecked();
  ScriptCompiler::CompilationDetails compilation_details;
  DirectHandle<SharedFunctionInfo> shared =
      Compiler::GetSharedFunctionInfoForScript(
          isolate, source_code, ScriptDetails(),
          v8::ScriptCompiler::kNoCompileOptions,
          ScriptCompiler::kNoCacheNoReason, NOT_NATIVES_CODE,
          &compilation_details)
          .ToHandleChecked();
  return Factory::JSFunctionBuilder{isolate, shared, isolate->native_context()}
      .Build();
}

TEST_F(LinkageTest, TestLinkageCreate) {
  Handle<JSFunction> function = Compile(isolate(), "a + b");
  Handle<SharedFunctionInfo> shared(function->shared(), isolate());
  OptimizedCompilationInfo info(zone(), isolate(), shared, function,
                                CodeKind::TURBOFAN_JS);
  auto call_descriptor = Linkage::ComputeIncoming(info.zone(), &info);
  ASSERT_NE(call_descriptor, nullptr);
}

TEST_F(LinkageTest, TestLinkageJSFunctionIncoming) {
  constexpr auto sources = make_test_sources();

  for (size_t index = 0; index < sources.size(); ++index) {
    const char* source = sources[index];
    Handle<JSFunction> function = Cast<JSFunction>(v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(CompileRun(v8_isolate(), source))));
    Handle<SharedFunctionInfo> shared(function->shared(), isolate());
    OptimizedCompilationInfo info(zone(), isolate(), shared, function,
                                  CodeKind::TURBOFAN_JS);
    auto call_descriptor = Linkage::ComputeIncoming(info.zone(), &info);
    ASSERT_NE(call_descriptor, nullptr);

    EXPECT_EQ(static_cast<int>(1 + index),
              static_cast<int>(call_descriptor->JSParameterCount()));
    EXPECT_EQ(1, static_cast<int>(call_descriptor->ReturnCount()));
    EXPECT_EQ(Operator::kNoProperties, call_descriptor->properties());
    EXPECT_TRUE(call_descriptor->IsJSFunctionCall());
  }
}

TEST_F(LinkageTest, TestLinkageJSCall) {
  Handle<JSFunction> function = Compile(isolate(), "a + c");
  Handle<SharedFunctionInfo> shared(function->shared(), isolate());
  OptimizedCompilationInfo info(zone(), isolate(), shared, function,
                                CodeKind::TURBOFAN_JS);

  // Test parameter counts from 0 to 10
  constexpr std::array<int, 11> param_counts = {0, 1, 2, 3, 4, 5,
                                                6, 7, 8, 9, 10};
  for (const auto param_count : param_counts) {
    auto call_descriptor = Linkage::GetJSCallDescriptor(
        info.zone(), false, param_count, CallDescriptor::kNoFlags);
    ASSERT_NE(call_descriptor, nullptr);
    EXPECT_EQ(param_count,
              static_cast<int>(call_descriptor->JSParameterCount()));
    EXPECT_EQ(1, static_cast<int>(call_descriptor->ReturnCount()));
    EXPECT_EQ(Operator::kNoProperties, call_descriptor->properties());
    EXPECT_TRUE(call_descriptor->IsJSFunctionCall());
  }
}

TEST_F(LinkageTest, TestLinkageRuntimeCall) {
  // TODO(titzer): test linkage creation for outgoing runtime calls.
}

TEST_F(LinkageTest, TestLinkageStubCall) {
  // TODO(bbudge) Add tests for FP registers.
  Isolate* isolate_ptr = isolate();
  Zone zone(isolate_ptr->allocator(), ZONE_NAME);
  Callable callable = Builtins::CallableFor(isolate_ptr, Builtin::kToNumber);
  OptimizedCompilationInfo info(base::ArrayVector("test"), &zone,
                                CodeKind::FOR_TESTING);
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      &zone, callable.descriptor(), 0, CallDescriptor::kNoFlags,
      Operator::kNoProperties);
  ASSERT_NE(call_descriptor, nullptr);
  EXPECT_EQ(0, static_cast<int>(call_descriptor->ParameterSlotCount()));
  EXPECT_EQ(1, static_cast<int>(call_descriptor->ReturnCount()));
  EXPECT_EQ(Operator::kNoProperties, call_descriptor->properties());
  EXPECT_FALSE(call_descriptor->IsJSFunctionCall());

  EXPECT_EQ(call_descriptor->GetParameterType(0), MachineType::AnyTagged());
  EXPECT_EQ(call_descriptor->GetReturnType(0), MachineType::AnyTagged());
  // TODO(titzer): test linkage creation for outgoing stub calls.
}

#if V8_ENABLE_WEBASSEMBLY
TEST_F(LinkageTest, TestFPLinkageStubCall) {
  Isolate* isolate_ptr = isolate();
  Zone zone(isolate_ptr->allocator(), ZONE_NAME);
  Callable callable =
      Builtins::CallableFor(isolate_ptr, Builtin::kWasmFloat64ToNumber);
  OptimizedCompilationInfo info(base::ArrayVector("test"), &zone,
                                CodeKind::FOR_TESTING);
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      &zone, callable.descriptor(), 0, CallDescriptor::kNoFlags,
      Operator::kNoProperties);
  ASSERT_NE(call_descriptor, nullptr);
  EXPECT_EQ(0, static_cast<int>(call_descriptor->ParameterSlotCount()));
  EXPECT_EQ(1, static_cast<int>(call_descriptor->ParameterCount()));
  EXPECT_EQ(1, static_cast<int>(call_descriptor->ReturnCount()));
  EXPECT_EQ(Operator::kNoProperties, call_descriptor->properties());
  EXPECT_FALSE(call_descriptor->IsJSFunctionCall());

  EXPECT_EQ(call_descriptor->GetInputType(1), MachineType::Float64());
  EXPECT_TRUE(call_descriptor->GetInputLocation(1).IsRegister());
  EXPECT_EQ(call_descriptor->GetReturnType(0), MachineType::AnyTagged());
  EXPECT_TRUE(call_descriptor->GetReturnLocation(0).IsRegister());
  EXPECT_EQ(call_descriptor->GetReturnLocation(0).GetLocation(),
            kReturnRegister0.code());
}
#endif  // V8_ENABLE_WEBASSEMBLY

}  // namespace v8::internal::compiler
