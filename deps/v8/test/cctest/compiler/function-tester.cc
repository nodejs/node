// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/function-tester.h"

#include "src/ast/ast-numbering.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/execution.h"
#include "src/full-codegen/full-codegen.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

FunctionTester::FunctionTester(const char* source, uint32_t flags)
    : isolate(main_isolate()),
      function((FLAG_allow_natives_syntax = true, NewFunction(source))),
      flags_(flags) {
  Compile(function);
  const uint32_t supported_flags = CompilationInfo::kNativeContextSpecializing |
                                   CompilationInfo::kInliningEnabled;
  CHECK_EQ(0u, flags_ & ~supported_flags);
}

FunctionTester::FunctionTester(Graph* graph, int param_count)
    : isolate(main_isolate()),
      function(NewFunction(BuildFunction(param_count).c_str())),
      flags_(0) {
  CompileGraph(graph);
}

FunctionTester::FunctionTester(Handle<Code> code, int param_count)
    : isolate(main_isolate()),
      function((FLAG_allow_natives_syntax = true,
                NewFunction(BuildFunction(param_count).c_str()))),
      flags_(0) {
  Compile(function);
  function->ReplaceCode(*code);
}

FunctionTester::FunctionTester(const CallInterfaceDescriptor& descriptor,
                               Handle<Code> code)
    : FunctionTester(code, descriptor.GetParameterCount()) {}

MaybeHandle<Object> FunctionTester::Call() {
  return Execution::Call(isolate, function, undefined(), 0, nullptr);
}

MaybeHandle<Object> FunctionTester::Call(Handle<Object> a) {
  Handle<Object> args[] = {a};
  return Execution::Call(isolate, function, undefined(), 1, args);
}

MaybeHandle<Object> FunctionTester::Call(Handle<Object> a, Handle<Object> b) {
  Handle<Object> args[] = {a, b};
  return Execution::Call(isolate, function, undefined(), 2, args);
}

MaybeHandle<Object> FunctionTester::Call(Handle<Object> a, Handle<Object> b,
                                         Handle<Object> c) {
  Handle<Object> args[] = {a, b, c};
  return Execution::Call(isolate, function, undefined(), 3, args);
}

MaybeHandle<Object> FunctionTester::Call(Handle<Object> a, Handle<Object> b,
                                         Handle<Object> c, Handle<Object> d) {
  Handle<Object> args[] = {a, b, c, d};
  return Execution::Call(isolate, function, undefined(), 4, args);
}

void FunctionTester::CheckThrows(Handle<Object> a, Handle<Object> b) {
  TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  MaybeHandle<Object> no_result = Call(a, b);
  CHECK(isolate->has_pending_exception());
  CHECK(try_catch.HasCaught());
  CHECK(no_result.is_null());
  isolate->OptionalRescheduleException(true);
}

v8::Local<v8::Message> FunctionTester::CheckThrowsReturnMessage(
    Handle<Object> a, Handle<Object> b) {
  TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  MaybeHandle<Object> no_result = Call(a, b);
  CHECK(isolate->has_pending_exception());
  CHECK(try_catch.HasCaught());
  CHECK(no_result.is_null());
  isolate->OptionalRescheduleException(true);
  CHECK(!try_catch.Message().IsEmpty());
  return try_catch.Message();
}

void FunctionTester::CheckCall(Handle<Object> expected, Handle<Object> a,
                               Handle<Object> b, Handle<Object> c,
                               Handle<Object> d) {
  Handle<Object> result = Call(a, b, c, d).ToHandleChecked();
  CHECK(expected->SameValue(*result));
}

Handle<JSFunction> FunctionTester::NewFunction(const char* source) {
  return Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CompileRun(source))));
}

Handle<JSObject> FunctionTester::NewObject(const char* source) {
  return Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(CompileRun(source))));
}

Handle<String> FunctionTester::Val(const char* string) {
  return isolate->factory()->InternalizeUtf8String(string);
}

Handle<Object> FunctionTester::Val(double value) {
  return isolate->factory()->NewNumber(value);
}

Handle<Object> FunctionTester::infinity() {
  return isolate->factory()->infinity_value();
}

Handle<Object> FunctionTester::minus_infinity() { return Val(-V8_INFINITY); }

Handle<Object> FunctionTester::nan() { return isolate->factory()->nan_value(); }

Handle<Object> FunctionTester::undefined() {
  return isolate->factory()->undefined_value();
}

Handle<Object> FunctionTester::null() {
  return isolate->factory()->null_value();
}

Handle<Object> FunctionTester::true_value() {
  return isolate->factory()->true_value();
}

Handle<Object> FunctionTester::false_value() {
  return isolate->factory()->false_value();
}

Handle<JSFunction> FunctionTester::ForMachineGraph(Graph* graph,
                                                   int param_count) {
  JSFunction* p = NULL;
  {  // because of the implicit handle scope of FunctionTester.
    FunctionTester f(graph, param_count);
    p = *f.function;
  }
  return Handle<JSFunction>(p);  // allocated in outer handle scope.
}

Handle<JSFunction> FunctionTester::Compile(Handle<JSFunction> function) {
  Zone zone(function->GetIsolate()->allocator());
  ParseInfo parse_info(&zone, function);
  CompilationInfo info(&parse_info, function);
  info.MarkAsDeoptimizationEnabled();

  if (!FLAG_turbo_from_bytecode) {
    CHECK(Parser::ParseStatic(info.parse_info()));
  }
  info.SetOptimizing();
  if (flags_ & CompilationInfo::kNativeContextSpecializing) {
    info.MarkAsNativeContextSpecializing();
  }
  if (flags_ & CompilationInfo::kInliningEnabled) {
    info.MarkAsInliningEnabled();
  }
  if (FLAG_turbo_from_bytecode) {
    CHECK(Compiler::EnsureBytecode(&info));
    info.MarkAsOptimizeFromBytecode();
  } else {
    CHECK(Compiler::Analyze(info.parse_info()));
    CHECK(Compiler::EnsureDeoptimizationSupport(&info));
  }
  JSFunction::EnsureLiterals(function);

  Handle<Code> code = Pipeline::GenerateCodeForTesting(&info);
  CHECK(!code.is_null());
  info.dependencies()->Commit(code);
  info.context()->native_context()->AddOptimizedCode(*code);
  function->ReplaceCode(*code);
  return function;
}

// Compile the given machine graph instead of the source of the function
// and replace the JSFunction's code with the result.
Handle<JSFunction> FunctionTester::CompileGraph(Graph* graph) {
  Zone zone(function->GetIsolate()->allocator());
  ParseInfo parse_info(&zone, function);
  CompilationInfo info(&parse_info, function);

  CHECK(Parser::ParseStatic(info.parse_info()));
  info.SetOptimizing();

  Handle<Code> code = Pipeline::GenerateCodeForTesting(&info, graph);
  CHECK(!code.is_null());
  function->ReplaceCode(*code);
  return function;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
