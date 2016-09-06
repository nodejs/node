// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_FUNCTION_TESTER_H_
#define V8_CCTEST_COMPILER_FUNCTION_TESTER_H_

#include "src/ast/ast-numbering.h"
#include "src/compiler.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/execution.h"
#include "src/full-codegen/full-codegen.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/rewriter.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

class FunctionTester : public InitializedHandleScope {
 public:
  explicit FunctionTester(const char* source, uint32_t flags = 0)
      : isolate(main_isolate()),
        function((FLAG_allow_natives_syntax = true, NewFunction(source))),
        flags_(flags) {
    Compile(function);
    const uint32_t supported_flags =
        CompilationInfo::kNativeContextSpecializing |
        CompilationInfo::kInliningEnabled;
    CHECK_EQ(0u, flags_ & ~supported_flags);
  }

  FunctionTester(Graph* graph, int param_count)
      : isolate(main_isolate()),
        function(NewFunction(BuildFunction(param_count).c_str())),
        flags_(0) {
    CompileGraph(graph);
  }

  FunctionTester(Handle<Code> code, int param_count)
      : isolate(main_isolate()),
        function((FLAG_allow_natives_syntax = true,
                  NewFunction(BuildFunction(param_count).c_str()))),
        flags_(0) {
    Compile(function);
    function->ReplaceCode(*code);
  }

  FunctionTester(const CallInterfaceDescriptor& descriptor, Handle<Code> code)
      : FunctionTester(code, descriptor.GetParameterCount()) {}

  Isolate* isolate;
  Handle<JSFunction> function;

  MaybeHandle<Object> Call() {
    return Execution::Call(isolate, function, undefined(), 0, nullptr);
  }

  MaybeHandle<Object> Call(Handle<Object> a) {
    Handle<Object> args[] = {a};
    return Execution::Call(isolate, function, undefined(), 1, args);
  }

  MaybeHandle<Object> Call(Handle<Object> a, Handle<Object> b) {
    Handle<Object> args[] = {a, b};
    return Execution::Call(isolate, function, undefined(), 2, args);
  }

  MaybeHandle<Object> Call(Handle<Object> a, Handle<Object> b,
                           Handle<Object> c) {
    Handle<Object> args[] = {a, b, c};
    return Execution::Call(isolate, function, undefined(), 3, args);
  }

  MaybeHandle<Object> Call(Handle<Object> a, Handle<Object> b, Handle<Object> c,
                           Handle<Object> d) {
    Handle<Object> args[] = {a, b, c, d};
    return Execution::Call(isolate, function, undefined(), 4, args);
  }

  void CheckThrows(Handle<Object> a, Handle<Object> b) {
    TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    MaybeHandle<Object> no_result = Call(a, b);
    CHECK(isolate->has_pending_exception());
    CHECK(try_catch.HasCaught());
    CHECK(no_result.is_null());
    isolate->OptionalRescheduleException(true);
  }

  v8::Local<v8::Message> CheckThrowsReturnMessage(Handle<Object> a,
                                                  Handle<Object> b) {
    TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    MaybeHandle<Object> no_result = Call(a, b);
    CHECK(isolate->has_pending_exception());
    CHECK(try_catch.HasCaught());
    CHECK(no_result.is_null());
    isolate->OptionalRescheduleException(true);
    CHECK(!try_catch.Message().IsEmpty());
    return try_catch.Message();
  }

  void CheckCall(Handle<Object> expected, Handle<Object> a, Handle<Object> b,
                 Handle<Object> c, Handle<Object> d) {
    Handle<Object> result = Call(a, b, c, d).ToHandleChecked();
    CHECK(expected->SameValue(*result));
  }

  void CheckCall(Handle<Object> expected, Handle<Object> a, Handle<Object> b,
                 Handle<Object> c) {
    return CheckCall(expected, a, b, c, undefined());
  }

  void CheckCall(Handle<Object> expected, Handle<Object> a, Handle<Object> b) {
    return CheckCall(expected, a, b, undefined());
  }

  void CheckCall(Handle<Object> expected, Handle<Object> a) {
    CheckCall(expected, a, undefined());
  }

  void CheckCall(Handle<Object> expected) { CheckCall(expected, undefined()); }

  void CheckCall(double expected, double a, double b) {
    CheckCall(Val(expected), Val(a), Val(b));
  }

  void CheckTrue(Handle<Object> a) { CheckCall(true_value(), a); }

  void CheckTrue(Handle<Object> a, Handle<Object> b) {
    CheckCall(true_value(), a, b);
  }

  void CheckTrue(Handle<Object> a, Handle<Object> b, Handle<Object> c) {
    CheckCall(true_value(), a, b, c);
  }

  void CheckTrue(Handle<Object> a, Handle<Object> b, Handle<Object> c,
                 Handle<Object> d) {
    CheckCall(true_value(), a, b, c, d);
  }

  void CheckTrue(double a, double b) {
    CheckCall(true_value(), Val(a), Val(b));
  }

  void CheckFalse(Handle<Object> a) { CheckCall(false_value(), a); }

  void CheckFalse(Handle<Object> a, Handle<Object> b) {
    CheckCall(false_value(), a, b);
  }

  void CheckFalse(double a, double b) {
    CheckCall(false_value(), Val(a), Val(b));
  }

  Handle<JSFunction> NewFunction(const char* source) {
    return Handle<JSFunction>::cast(v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(CompileRun(source))));
  }

  Handle<JSObject> NewObject(const char* source) {
    return Handle<JSObject>::cast(v8::Utils::OpenHandle(
        *v8::Local<v8::Object>::Cast(CompileRun(source))));
  }

  Handle<String> Val(const char* string) {
    return isolate->factory()->InternalizeUtf8String(string);
  }

  Handle<Object> Val(double value) {
    return isolate->factory()->NewNumber(value);
  }

  Handle<Object> infinity() { return isolate->factory()->infinity_value(); }

  Handle<Object> minus_infinity() { return Val(-V8_INFINITY); }

  Handle<Object> nan() { return isolate->factory()->nan_value(); }

  Handle<Object> undefined() { return isolate->factory()->undefined_value(); }

  Handle<Object> null() { return isolate->factory()->null_value(); }

  Handle<Object> true_value() { return isolate->factory()->true_value(); }

  Handle<Object> false_value() { return isolate->factory()->false_value(); }

  static Handle<JSFunction> ForMachineGraph(Graph* graph, int param_count) {
    JSFunction* p = NULL;
    {  // because of the implicit handle scope of FunctionTester.
      FunctionTester f(graph, param_count);
      p = *f.function;
    }
    return Handle<JSFunction>(p);  // allocated in outer handle scope.
  }

 private:
  uint32_t flags_;

  Handle<JSFunction> Compile(Handle<JSFunction> function) {
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

  std::string BuildFunction(int param_count) {
    std::string function_string = "(function(";
    if (param_count > 0) {
      function_string += 'a';
      for (int i = 1; i < param_count; i++) {
        function_string += ',';
        function_string += static_cast<char>('a' + i);
      }
    }
    function_string += "){})";
    return function_string;
  }

  // Compile the given machine graph instead of the source of the function
  // and replace the JSFunction's code with the result.
  Handle<JSFunction> CompileGraph(Graph* graph) {
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
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_FUNCTION_TESTER_H_
