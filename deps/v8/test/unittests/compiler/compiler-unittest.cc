// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/compiler.h"

#include <stdlib.h>
#include <wchar.h>

#include <memory>

#include "include/v8-function.h"
#include "include/v8-local-handle.h"
#include "include/v8-profiler.h"
#include "include/v8-script.h"
#include "src/api/api-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/script-details.h"
#include "src/heap/factory.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "test/unittests/heap/heap-utils.h"  // For ManualGCScope.
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

using CompilerTest = TestWithContext;
namespace internal {

static Handle<Object> GetGlobalProperty(const char* name) {
  Isolate* isolate = reinterpret_cast<i::Isolate*>(v8::Isolate::GetCurrent());
  return JSReceiver::GetProperty(isolate, isolate->global_object(), name)
      .ToHandleChecked();
}

static void SetGlobalProperty(const char* name, Tagged<Object> value) {
  Isolate* isolate = reinterpret_cast<i::Isolate*>(v8::Isolate::GetCurrent());
  Handle<Object> object(value, isolate);
  Handle<String> internalized_name =
      isolate->factory()->InternalizeUtf8String(name);
  Handle<JSObject> global(isolate->context()->global_object(), isolate);
  Runtime::SetObjectProperty(isolate, global, internalized_name, object,
                             StoreOrigin::kMaybeKeyed, Just(kDontThrow))
      .Check();
}

static Handle<JSFunction> Compile(const char* source) {
  Isolate* isolate = reinterpret_cast<i::Isolate*>(v8::Isolate::GetCurrent());
  Handle<String> source_code = isolate->factory()
                                   ->NewStringFromUtf8(base::CStrVector(source))
                                   .ToHandleChecked();
  ScriptCompiler::CompilationDetails compilation_details;
  Handle<SharedFunctionInfo> shared =
      Compiler::GetSharedFunctionInfoForScript(
          isolate, source_code, ScriptDetails(),
          v8::ScriptCompiler::kNoCompileOptions,
          ScriptCompiler::kNoCacheNoReason, NOT_NATIVES_CODE,
          &compilation_details)
          .ToHandleChecked();
  return Factory::JSFunctionBuilder{isolate, shared, isolate->native_context()}
      .Build();
}

static double Inc(Isolate* isolate, int x) {
  const char* source = "result = %d + 1;";
  base::EmbeddedVector<char, 512> buffer;
  SNPrintF(buffer, source, x);

  Handle<JSFunction> fun = Compile(buffer.begin());
  if (fun.is_null()) return -1;

  Handle<JSObject> global(isolate->context()->global_object(), isolate);
  Execution::CallScript(isolate, fun, global,
                        isolate->factory()->empty_fixed_array())
      .Check();
  return Object::NumberValue(*GetGlobalProperty("result"));
}

TEST_F(CompilerTest, Inc) {
  v8::HandleScope scope(isolate());
  EXPECT_EQ(4.0, Inc(i_isolate(), 3));
}

static double Add(Isolate* isolate, int x, int y) {
  Handle<JSFunction> fun = Compile("result = x + y;");
  if (fun.is_null()) return -1;

  SetGlobalProperty("x", Smi::FromInt(x));
  SetGlobalProperty("y", Smi::FromInt(y));
  Handle<JSObject> global(isolate->context()->global_object(), isolate);
  Execution::CallScript(isolate, fun, global,
                        isolate->factory()->empty_fixed_array())
      .Check();
  return Object::NumberValue(*GetGlobalProperty("result"));
}

TEST_F(CompilerTest, Add) {
  v8::HandleScope scope(isolate());
  EXPECT_EQ(5.0, Add(i_isolate(), 2, 3));
}

static double Abs(Isolate* isolate, int x) {
  Handle<JSFunction> fun = Compile("if (x < 0) result = -x; else result = x;");
  if (fun.is_null()) return -1;

  SetGlobalProperty("x", Smi::FromInt(x));
  Handle<JSObject> global(isolate->context()->global_object(), isolate);
  Execution::CallScript(isolate, fun, global,
                        isolate->factory()->empty_fixed_array())
      .Check();
  return Object::NumberValue(*GetGlobalProperty("result"));
}

TEST_F(CompilerTest, Abs) {
  v8::HandleScope scope(isolate());
  EXPECT_EQ(3.0, Abs(i_isolate(), -3));
}

static double Sum(Isolate* isolate, int n) {
  Handle<JSFunction> fun =
      Compile("s = 0; while (n > 0) { s += n; n -= 1; }; result = s;");
  if (fun.is_null()) return -1;

  SetGlobalProperty("n", Smi::FromInt(n));
  Handle<JSObject> global(isolate->context()->global_object(), isolate);
  Execution::CallScript(isolate, fun, global,
                        isolate->factory()->empty_fixed_array())
      .Check();
  return Object::NumberValue(*GetGlobalProperty("result"));
}

TEST_F(CompilerTest, Sum) {
  v8::HandleScope scope(isolate());
  EXPECT_EQ(5050.0, Sum(i_isolate(), 100));
}

using CompilerPrintTest = WithPrintExtensionMixin<v8::TestWithIsolate>;

TEST_F(CompilerPrintTest, Print) {
  v8::HandleScope scope(isolate());
  const char* extension_names[1] = {
      WithPrintExtensionMixin::kPrintExtensionName};
  v8::ExtensionConfiguration config(1, extension_names);
  v8::Local<v8::Context> context = v8::Context::New(isolate(), &config);
  v8::Context::Scope context_scope(context);
  const char* source = "for (n = 0; n < 100; ++n) print(n, 1, 2);";
  Handle<JSFunction> fun = Compile(source);
  if (fun.is_null()) return;
  Handle<JSObject> global(i_isolate()->context()->global_object(), i_isolate());
  Execution::CallScript(i_isolate(), fun, global,
                        i_isolate()->factory()->empty_fixed_array())
      .Check();
}

// The following test method stems from my coding efforts today. It
// tests all the functionality I have added to the compiler today
TEST_F(CompilerTest, Stuff) {
  v8::HandleScope scope(isolate());
  const char* source =
      "r = 0;\n"
      "a = new Object;\n"
      "if (a == a) r+=1;\n"             // 1
      "if (a != new Object()) r+=2;\n"  // 2
      "a.x = 42;\n"
      "if (a.x == 42) r+=4;\n"  // 4
      "function foo() { var x = 87; return x; }\n"
      "if (foo() == 87) r+=8;\n"  // 8
      "function bar() { var x; x = 99; return x; }\n"
      "if (bar() == 99) r+=16;\n"  // 16
      "function baz() { var x = 1, y, z = 2; y = 3; return x + y + z; }\n"
      "if (baz() == 6) r+=32;\n"  // 32
      "function Cons0() { this.x = 42; this.y = 87; }\n"
      "if (new Cons0().x == 42) r+=64;\n"   // 64
      "if (new Cons0().y == 87) r+=128;\n"  // 128
      "function Cons2(x, y) { this.sum = x + y; }\n"
      "if (new Cons2(3,4).sum == 7) r+=256;";  // 256

  Handle<JSFunction> fun = Compile(source);
  EXPECT_TRUE(!fun.is_null());
  Handle<JSObject> global(i_isolate()->context()->global_object(), i_isolate());
  Execution::CallScript(i_isolate(), fun, global,
                        i_isolate()->factory()->empty_fixed_array())
      .Check();
  EXPECT_EQ(511.0, Object::NumberValue(*GetGlobalProperty("r")));
}

TEST_F(CompilerTest, UncaughtThrow) {
  v8::HandleScope scope(isolate());

  const char* source = "throw 42;";
  Handle<JSFunction> fun = Compile(source);
  EXPECT_TRUE(!fun.is_null());
  Isolate* isolate = fun->GetIsolate();
  Handle<JSObject> global(isolate->context()->global_object(), isolate);
  EXPECT_TRUE(Execution::CallScript(isolate, fun, global,
                                    isolate->factory()->empty_fixed_array())
                  .is_null());
  EXPECT_EQ(42.0, Object::NumberValue(isolate->exception()));
}

using CompilerC2JSFramesTest = WithPrintExtensionMixin<v8::TestWithIsolate>;

// Tests calling a builtin function from C/C++ code, and the builtin function
// performs GC. It creates a stack frame looks like following:
//   | C (PerformGC) |
//   |   JS-to-C     |
//   |      JS       |
//   |   C-to-JS     |
TEST_F(CompilerC2JSFramesTest, C2JSFrames) {
  v8_flags.expose_gc = true;
  v8::HandleScope scope(isolate());
  const char* extension_names[2] = {
      "v8/gc", WithPrintExtensionMixin::kPrintExtensionName};
  v8::ExtensionConfiguration config(2, extension_names);
  v8::Local<v8::Context> context = v8::Context::New(isolate(), &config);
  v8::Context::Scope context_scope(context);

  const char* source = "function foo(a) { gc(), print(a); }";

  Handle<JSFunction> fun0 = Compile(source);
  EXPECT_TRUE(!fun0.is_null());
  Isolate* isolate = fun0->GetIsolate();

  // Run the generated code to populate the global object with 'foo'.
  Handle<JSObject> global(isolate->context()->global_object(), isolate);
  Execution::CallScript(isolate, fun0, global,
                        isolate->factory()->empty_fixed_array())
      .Check();

  Handle<Object> fun1 =
      JSReceiver::GetProperty(isolate, isolate->global_object(), "foo")
          .ToHandleChecked();
  EXPECT_TRUE(IsJSFunction(*fun1));

  Handle<Object> argv[] = {
      isolate->factory()->InternalizeString(base::StaticCharVector("hello"))};
  Execution::Call(isolate, Cast<JSFunction>(fun1), global, arraysize(argv),
                  argv)
      .Check();
}

// Regression 236. Calling InitLineEnds on a Script with undefined
// source resulted in crash.
TEST_F(CompilerTest, Regression236) {
  Factory* factory = i_isolate()->factory();
  v8::HandleScope scope(isolate());

  DirectHandle<Script> script = factory->NewScript(factory->undefined_value());
  EXPECT_EQ(-1, Script::GetLineNumber(script, 0));
  EXPECT_EQ(-1, Script::GetLineNumber(script, 100));
  EXPECT_EQ(-1, Script::GetLineNumber(script, -1));
}

TEST_F(CompilerTest, GetScriptLineNumber) {
  v8::HandleScope scope(isolate());
  v8::ScriptOrigin origin = v8::ScriptOrigin(NewString("test"));
  const char function_f[] = "function f() {}";
  const int max_rows = 1000;
  const int buffer_size = max_rows + sizeof(function_f);
  base::ScopedVector<char> buffer(buffer_size);
  memset(buffer.begin(), '\n', buffer_size - 1);
  buffer[buffer_size - 1] = '\0';

  for (int i = 0; i < max_rows; ++i) {
    if (i > 0) buffer[i - 1] = '\n';
    MemCopy(&buffer[i], function_f, sizeof(function_f) - 1);
    v8::Local<v8::String> script_body = NewString(buffer.begin());
    v8::Script::Compile(context(), script_body, &origin)
        .ToLocalChecked()
        ->Run(context())
        .ToLocalChecked();
    v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
        context()->Global()->Get(context(), NewString("f")).ToLocalChecked());
    EXPECT_EQ(i, f->GetScriptLineNumber());
  }
}

TEST_F(CompilerTest, FeedbackVectorPreservedAcrossRecompiles) {
  if (i::v8_flags.always_turbofan || !i::v8_flags.turbofan) return;
  i::v8_flags.allow_natives_syntax = true;
  if (!i_isolate()->use_optimizer()) return;
  v8::HandleScope scope(isolate());

  // Make sure function f has a call that uses a type feedback slot.
  RunJS(
      "function fun() {};"
      "fun1 = fun;"
      "%PrepareFunctionForOptimization(f);"
      "function f(a) { a(); } f(fun1);");

  DirectHandle<JSFunction> f = Cast<
      JSFunction>(v8::Utils::OpenDirectHandle(*v8::Local<v8::Function>::Cast(
      context()->Global()->Get(context(), NewString("f")).ToLocalChecked())));

  // Verify that we gathered feedback.
  DirectHandle<FeedbackVector> feedback_vector(f->feedback_vector(),
                                               f->GetIsolate());
  EXPECT_TRUE(!feedback_vector->is_empty());
  FeedbackSlot slot_for_a(0);
  Tagged<MaybeObject> object = feedback_vector->Get(slot_for_a);
  {
    Tagged<HeapObject> heap_object;
    EXPECT_TRUE(object.GetHeapObjectIfWeak(&heap_object));
    EXPECT_TRUE(IsJSFunction(heap_object));
  }

  RunJS("%OptimizeFunctionOnNextCall(f); f(fun1);");

  // Verify that the feedback is still "gathered" despite a recompilation
  // of the full code.
  EXPECT_TRUE(f->HasAttachedOptimizedCode(i_isolate()));
  object = f->feedback_vector()->Get(slot_for_a);
  {
    Tagged<HeapObject> heap_object;
    EXPECT_TRUE(object.GetHeapObjectIfWeak(&heap_object));
    EXPECT_TRUE(IsJSFunction(heap_object));
  }
}

TEST_F(CompilerTest, FeedbackVectorUnaffectedByScopeChanges) {
  if (i::v8_flags.always_turbofan || !i::v8_flags.lazy ||
      i::v8_flags.lite_mode) {
    return;
  }
  v8::HandleScope scope(isolate());

  RunJS(
      "function builder() {"
      "  call_target = function() { return 3; };"
      "  return (function() {"
      "    eval('');"
      "    return function() {"
      "      'use strict';"
      "      call_target();"
      "    }"
      "  })();"
      "}"
      "morphing_call = builder();");

  DirectHandle<JSFunction> f = Cast<JSFunction>(
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Function>::Cast(
          context()
              ->Global()
              ->Get(context(), NewString("morphing_call"))
              .ToLocalChecked())));

  // If we are compiling lazily then it should not be compiled, and so no
  // feedback vector allocated yet.
  EXPECT_TRUE(!f->shared()->is_compiled());

  RunJS("morphing_call();");

  // Now a feedback vector / closure feedback cell array is allocated.
  EXPECT_TRUE(f->shared()->is_compiled());
  EXPECT_TRUE(f->has_feedback_vector() || f->has_closure_feedback_cell_array());
}

// Test that optimized code for different closures is actually shared.
TEST_F(CompilerTest, OptimizedCodeSharing1) {
  v8_flags.stress_compaction = false;
  v8_flags.allow_natives_syntax = true;
  v8::HandleScope scope(isolate());
  for (int i = 0; i < 3; i++) {
    context()
        ->Global()
        ->Set(context(), NewString("x"), v8::Integer::New(isolate(), i))
        .FromJust();
    RunJS(
        "function MakeClosure() {"
        "  return function() { return x; };"
        "}"
        "var closure0 = MakeClosure();"
        "var closure1 = MakeClosure();"  // We only share optimized code
                                         // if there are at least two closures.
        "%PrepareFunctionForOptimization(closure0);"
        "%DebugPrint(closure0());"
        "%OptimizeFunctionOnNextCall(closure0);"
        "%DebugPrint(closure0());"
        "closure1();"
        "var closure2 = MakeClosure(); closure2();");
    DirectHandle<JSFunction> fun1 = Cast<JSFunction>(
        v8::Utils::OpenDirectHandle(*v8::Local<v8::Function>::Cast(
            context()
                ->Global()
                ->Get(context(), NewString("closure1"))
                .ToLocalChecked())));
    DirectHandle<JSFunction> fun2 = Cast<JSFunction>(
        v8::Utils::OpenDirectHandle(*v8::Local<v8::Function>::Cast(
            context()
                ->Global()
                ->Get(context(), NewString("closure2"))
                .ToLocalChecked())));
    EXPECT_TRUE(fun1->HasAttachedOptimizedCode(i_isolate()) ||
                !i_isolate()->use_optimizer());
    EXPECT_TRUE(fun2->HasAttachedOptimizedCode(i_isolate()) ||
                !i_isolate()->use_optimizer());
    EXPECT_EQ(fun1->code(i_isolate()), fun2->code(i_isolate()));
  }
}

TEST_F(CompilerTest, CompileFunction) {
  if (i::v8_flags.always_turbofan) return;
  v8::HandleScope scope(isolate());
  RunJS("var r = 10;");
  v8::Local<v8::Object> math = v8::Local<v8::Object>::Cast(
      context()->Global()->Get(context(), NewString("Math")).ToLocalChecked());
  v8::ScriptCompiler::Source script_source(
      NewString("a = PI * r * r;"
                "x = r * cos(PI);"
                "y = r * sin(PI / 2);"));
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunction(context(), &script_source, 0, nullptr,
                                          1, &math)
          .ToLocalChecked();
  EXPECT_TRUE(!fun.IsEmpty());

  i::DisallowCompilation no_compile(i_isolate());
  fun->Call(context(), context()->Global(), 0, nullptr).ToLocalChecked();
  EXPECT_TRUE(context()->Global()->Has(context(), NewString("a")).FromJust());
  v8::Local<v8::Value> a =
      context()->Global()->Get(context(), NewString("a")).ToLocalChecked();
  EXPECT_TRUE(a->IsNumber());
  EXPECT_TRUE(context()->Global()->Has(context(), NewString("x")).FromJust());
  v8::Local<v8::Value> x =
      context()->Global()->Get(context(), NewString("x")).ToLocalChecked();
  EXPECT_TRUE(x->IsNumber());
  EXPECT_TRUE(context()->Global()->Has(context(), NewString("y")).FromJust());
  v8::Local<v8::Value> y =
      context()->Global()->Get(context(), NewString("y")).ToLocalChecked();
  EXPECT_TRUE(y->IsNumber());
  EXPECT_EQ(314.1592653589793, a->NumberValue(context()).FromJust());
  EXPECT_EQ(-10.0, x->NumberValue(context()).FromJust());
  EXPECT_EQ(10.0, y->NumberValue(context()).FromJust());
}

TEST_F(CompilerTest, CompileFunctionComplex) {
  v8::HandleScope scope(isolate());
  RunJS(
      "var x = 1;"
      "var y = 2;"
      "var z = 4;"
      "var a = {x: 8, y: 16};"
      "var b = {x: 32};");
  v8::Local<v8::Object> ext[2];
  ext[0] = v8::Local<v8::Object>::Cast(
      context()->Global()->Get(context(), NewString("a")).ToLocalChecked());
  ext[1] = v8::Local<v8::Object>::Cast(
      context()->Global()->Get(context(), NewString("b")).ToLocalChecked());
  v8::ScriptCompiler::Source script_source(NewString("result = x + y + z"));
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunction(context(), &script_source, 0, nullptr,
                                          2, ext)
          .ToLocalChecked();
  EXPECT_TRUE(!fun.IsEmpty());
  fun->Call(context(), context()->Global(), 0, nullptr).ToLocalChecked();
  EXPECT_TRUE(
      context()->Global()->Has(context(), NewString("result")).FromJust());
  v8::Local<v8::Value> result =
      context()->Global()->Get(context(), NewString("result")).ToLocalChecked();
  EXPECT_TRUE(result->IsNumber());
  EXPECT_EQ(52.0, result->NumberValue(context()).FromJust());
}

TEST_F(CompilerTest, CompileFunctionArgs) {
  v8::HandleScope scope(isolate());
  RunJS("var a = {x: 23};");
  v8::Local<v8::Object> ext[1];
  ext[0] = v8::Local<v8::Object>::Cast(
      context()->Global()->Get(context(), NewString("a")).ToLocalChecked());
  v8::ScriptCompiler::Source script_source(NewString("result = x + abc"));
  v8::Local<v8::String> arg = NewString("abc");
  v8::Local<v8::Function> fun = v8::ScriptCompiler::CompileFunction(
                                    context(), &script_source, 1, &arg, 1, ext)
                                    .ToLocalChecked();
  EXPECT_EQ(1, fun->Get(context(), NewString("length"))
                   .ToLocalChecked()
                   ->ToInt32(context())
                   .ToLocalChecked()
                   ->Value());
  v8::Local<v8::Value> arg_value = v8::Number::New(isolate(), 42.0);
  fun->Call(context(), context()->Global(), 1, &arg_value).ToLocalChecked();
  EXPECT_TRUE(
      context()->Global()->Has(context(), NewString("result")).FromJust());
  v8::Local<v8::Value> result =
      context()->Global()->Get(context(), NewString("result")).ToLocalChecked();
  EXPECT_TRUE(result->IsNumber());
  EXPECT_EQ(65.0, result->NumberValue(context()).FromJust());
}

TEST_F(CompilerTest, CompileFunctionComments) {
  v8::HandleScope scope(isolate());
  RunJS("var a = {x: 23, y: 1, z: 2};");
  v8::Local<v8::Object> ext[1];
  ext[0] = v8::Local<v8::Object>::Cast(
      context()->Global()->Get(context(), NewString("a")).ToLocalChecked());
  v8::Local<v8::String> source =
      RunJS("'result = /* y + */ x + a\\u4e00 // + z'").As<v8::String>();
  v8::ScriptCompiler::Source script_source(source);
  v8::Local<v8::String> arg = RunJS("'a\\u4e00'").As<v8::String>();
  v8::Local<v8::Function> fun = v8::ScriptCompiler::CompileFunction(
                                    context(), &script_source, 1, &arg, 1, ext)
                                    .ToLocalChecked();
  EXPECT_TRUE(!fun.IsEmpty());
  v8::Local<v8::Value> arg_value = v8::Number::New(isolate(), 42.0);
  fun->Call(context(), context()->Global(), 1, &arg_value).ToLocalChecked();
  EXPECT_TRUE(
      context()->Global()->Has(context(), NewString("result")).FromJust());
  v8::Local<v8::Value> result =
      context()->Global()->Get(context(), NewString("result")).ToLocalChecked();
  EXPECT_TRUE(result->IsNumber());
  EXPECT_EQ(65.0, result->NumberValue(context()).FromJust());
}

TEST_F(CompilerTest, CompileFunctionNonIdentifierArgs) {
  v8::HandleScope scope(isolate());
  v8::ScriptCompiler::Source script_source(NewString("result = 1"));
  v8::Local<v8::String> arg = NewString("b }");
  EXPECT_TRUE(
      v8::ScriptCompiler::CompileFunction(context(), &script_source, 1, &arg)
          .IsEmpty());
}

TEST_F(CompilerTest, CompileFunctionRenderCallSite) {
  v8::HandleScope scope(isolate());
  static const char* source1 =
      "try {"
      "  var a = [];"
      "  a[0]();"
      "} catch (e) {"
      "  return e.toString();"
      "}";
  static const char* expect1 = "TypeError: a[0] is not a function";
  static const char* source2 =
      "try {"
      "  (function() {"
      "    var a = [];"
      "    a[0]();"
      "  })()"
      "} catch (e) {"
      "  return e.toString();"
      "}";
  static const char* expect2 = "TypeError: a[0] is not a function";
  {
    v8::ScriptCompiler::Source script_source(NewString(source1));
    v8::Local<v8::Function> fun =
        v8::ScriptCompiler::CompileFunction(context(), &script_source)
            .ToLocalChecked();
    EXPECT_TRUE(!fun.IsEmpty());
    v8::Local<v8::Value> result =
        fun->Call(context(), context()->Global(), 0, nullptr).ToLocalChecked();
    EXPECT_TRUE(result->IsString());
    EXPECT_TRUE(v8::Local<v8::String>::Cast(result)
                    ->Equals(context(), NewString(expect1))
                    .FromJust());
  }
  {
    v8::ScriptCompiler::Source script_source(NewString(source2));
    v8::Local<v8::Function> fun =
        v8::ScriptCompiler::CompileFunction(context(), &script_source)
            .ToLocalChecked();
    v8::Local<v8::Value> result =
        fun->Call(context(), context()->Global(), 0, nullptr).ToLocalChecked();
    EXPECT_TRUE(result->IsString());
    EXPECT_TRUE(v8::Local<v8::String>::Cast(result)
                    ->Equals(context(), NewString(expect2))
                    .FromJust());
  }
}

TEST_F(CompilerTest, CompileFunctionQuirks) {
  v8::HandleScope scope(isolate());
  {
    static const char* source =
        "[x, y] = ['ab', 'cd'];"
        "return x + y";
    static const char* expect = "abcd";
    v8::ScriptCompiler::Source script_source(NewString(source));
    v8::Local<v8::Function> fun =
        v8::ScriptCompiler::CompileFunction(context(), &script_source)
            .ToLocalChecked();
    v8::Local<v8::Value> result =
        fun->Call(context(), context()->Global(), 0, nullptr).ToLocalChecked();
    EXPECT_TRUE(result->IsString());
    EXPECT_TRUE(v8::Local<v8::String>::Cast(result)
                    ->Equals(context(), NewString(expect))
                    .FromJust());
  }
  {
    static const char* source = "'use strict'; var a = 077";
    v8::ScriptCompiler::Source script_source(NewString(source));
    v8::TryCatch try_catch(isolate());
    EXPECT_TRUE(v8::ScriptCompiler::CompileFunction(context(), &script_source)
                    .IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }
  {
    static const char* source = "{ let x; { var x } }";
    v8::ScriptCompiler::Source script_source(NewString(source));
    v8::TryCatch try_catch(isolate());
    EXPECT_TRUE(v8::ScriptCompiler::CompileFunction(context(), &script_source)
                    .IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }
}

TEST_F(CompilerTest, CompileFunctionScriptOrigin) {
  v8::HandleScope scope(isolate());
  v8::ScriptOrigin origin(NewString("test"), 22, 41);
  v8::ScriptCompiler::Source script_source(NewString("throw new Error()"),
                                           origin);
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunction(context(), &script_source)
          .ToLocalChecked();
  EXPECT_TRUE(!fun.IsEmpty());
  auto fun_i = i::Cast<i::JSFunction>(Utils::OpenHandle(*fun));
  EXPECT_TRUE(IsSharedFunctionInfo(fun_i->shared()));
  EXPECT_TRUE(
      Utils::ToLocal(
          i::handle(i::Cast<i::Script>(fun_i->shared()->script())->name(),
                    i_isolate()))
          ->StrictEquals(NewString("test")));
  v8::TryCatch try_catch(isolate());
  isolate()->SetCaptureStackTraceForUncaughtExceptions(true);
  EXPECT_TRUE(fun->Call(context(), context()->Global(), 0, nullptr).IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_TRUE(!try_catch.Exception().IsEmpty());
  v8::Local<v8::StackTrace> stack =
      v8::Exception::GetStackTrace(try_catch.Exception());
  EXPECT_TRUE(!stack.IsEmpty());
  EXPECT_GT(stack->GetFrameCount(), 0);
  v8::Local<v8::StackFrame> frame = stack->GetFrame(isolate(), 0);
  EXPECT_EQ(23, frame->GetLineNumber());
  EXPECT_EQ(42 + strlen("throw "), static_cast<unsigned>(frame->GetColumn()));
}

TEST_F(CompilerTest, CompileFunctionFunctionToString) {
#define CHECK_NOT_CAUGHT(__local_context__, try_catch, __op__)                 \
  do {                                                                         \
    const char* op = (__op__);                                                 \
    if (try_catch.HasCaught()) {                                               \
      v8::String::Utf8Value error(isolate(), try_catch.Exception()             \
                                                 ->ToString(__local_context__) \
                                                 .ToLocalChecked());           \
      FATAL("Unexpected exception thrown during %s:\n\t%s\n", op, *error);     \
    }                                                                          \
  } while (false)

  {
    v8::HandleScope scope(isolate());

    // Regression test for v8:6190
    {
      v8::ScriptOrigin origin(NewString("test"), 22, 41);
      v8::ScriptCompiler::Source script_source(NewString("return event"),
                                               origin);

      v8::Local<v8::String> params[] = {NewString("event")};
      v8::TryCatch try_catch(isolate());
      v8::MaybeLocal<v8::Function> maybe_fun =
          v8::ScriptCompiler::CompileFunction(context(), &script_source,
                                              arraysize(params), params);

      CHECK_NOT_CAUGHT(context(), try_catch,
                       "v8::ScriptCompiler::CompileFunction");

      v8::Local<v8::Function> fun = maybe_fun.ToLocalChecked();
      EXPECT_TRUE(!fun.IsEmpty());
      EXPECT_TRUE(!try_catch.HasCaught());
      v8::Local<v8::String> result = fun->ToString(context()).ToLocalChecked();
      v8::Local<v8::String> expected = NewString(
          "function (event) {\n"
          "return event\n"
          "}");
      EXPECT_TRUE(expected->Equals(context(), result).FromJust());
    }

    // With no parameters:
    {
      v8::ScriptOrigin origin(NewString("test"), 17, 31);
      v8::ScriptCompiler::Source script_source(NewString("return 0"), origin);

      v8::TryCatch try_catch(isolate());
      v8::MaybeLocal<v8::Function> maybe_fun =
          v8::ScriptCompiler::CompileFunction(context(), &script_source);

      CHECK_NOT_CAUGHT(context(), try_catch,
                       "v8::ScriptCompiler::CompileFunction");

      v8::Local<v8::Function> fun = maybe_fun.ToLocalChecked();
      EXPECT_TRUE(!fun.IsEmpty());
      EXPECT_TRUE(!try_catch.HasCaught());
      v8::Local<v8::String> result = fun->ToString(context()).ToLocalChecked();
      v8::Local<v8::String> expected = NewString(
          "function () {\n"
          "return 0\n"
          "}");
      EXPECT_TRUE(expected->Equals(context(), result).FromJust());
    }

    // With a name:
    {
      v8::ScriptOrigin origin(NewString("test"), 17, 31);
      v8::ScriptCompiler::Source script_source(NewString("return 0"), origin);

      v8::TryCatch try_catch(isolate());
      v8::MaybeLocal<v8::Function> maybe_fun =
          v8::ScriptCompiler::CompileFunction(context(), &script_source);

      CHECK_NOT_CAUGHT(context(), try_catch,
                       "v8::ScriptCompiler::CompileFunction");

      v8::Local<v8::Function> fun = maybe_fun.ToLocalChecked();
      EXPECT_TRUE(!fun.IsEmpty());
      EXPECT_TRUE(!try_catch.HasCaught());

      fun->SetName(NewString("onclick"));

      v8::Local<v8::String> result = fun->ToString(context()).ToLocalChecked();
      v8::Local<v8::String> expected = NewString(
          "function onclick() {\n"
          "return 0\n"
          "}");
      EXPECT_TRUE(expected->Equals(context(), result).FromJust());
    }
  }
#undef CHECK_NOT_CAUGHT
}

TEST_F(CompilerTest, InvocationCount) {
  if (v8_flags.lite_mode) return;
  v8_flags.allow_natives_syntax = true;
  v8_flags.always_turbofan = false;
  v8::HandleScope scope(isolate());

  RunJS(
      "function bar() {};"
      "%EnsureFeedbackVectorForFunction(bar);"
      "function foo() { return bar(); };"
      "%EnsureFeedbackVectorForFunction(foo);"
      "foo();");
  DirectHandle<JSFunction> foo = Cast<JSFunction>(GetGlobalProperty("foo"));
  EXPECT_EQ(1, foo->feedback_vector()->invocation_count());
  RunJS("foo()");
  EXPECT_EQ(2, foo->feedback_vector()->invocation_count());
  RunJS("bar()");
  EXPECT_EQ(2, foo->feedback_vector()->invocation_count());
  RunJS("foo(); foo()");
  EXPECT_EQ(4, foo->feedback_vector()->invocation_count());
}

TEST_F(CompilerTest, ShallowEagerCompilation) {
  i::v8_flags.always_turbofan = false;
  v8::HandleScope scope(isolate());
  v8::Local<v8::String> source = NewString(
      "function f(x) {"
      "  return x + x;"
      "}"
      "f(2)");
  v8::ScriptCompiler::Source script_source(source);
  v8::Local<v8::Script> script =
      v8::ScriptCompiler::Compile(context(), &script_source,
                                  v8::ScriptCompiler::kEagerCompile)
          .ToLocalChecked();
  {
    v8::internal::DisallowCompilation no_compile_expected(i_isolate());
    v8::Local<v8::Value> result = script->Run(context()).ToLocalChecked();
    EXPECT_EQ(4, result->Int32Value(context()).FromJust());
  }
}

TEST_F(CompilerTest, DeepEagerCompilation) {
  i::v8_flags.always_turbofan = false;
  v8::HandleScope scope(isolate());
  v8::Local<v8::String> source = NewString(
      "function f(x) {"
      "  function g(x) {"
      "    function h(x) {"
      "      return x ** x;"
      "    }"
      "    return h(x) * h(x);"
      "  }"
      "  return g(x) + g(x);"
      "}"
      "f(2)");
  v8::ScriptCompiler::Source script_source(source);
  v8::Local<v8::Script> script =
      v8::ScriptCompiler::Compile(context(), &script_source,
                                  v8::ScriptCompiler::kEagerCompile)
          .ToLocalChecked();
  {
    v8::internal::DisallowCompilation no_compile_expected(i_isolate());
    v8::Local<v8::Value> result = script->Run(context()).ToLocalChecked();
    EXPECT_EQ(32, result->Int32Value(context()).FromJust());
  }
}

TEST_F(CompilerTest, DeepEagerCompilationPeakMemory) {
  i::v8_flags.always_turbofan = false;
  v8::HandleScope scope(isolate());
  v8::Local<v8::String> source = NewString(
      "function f() {"
      "  function g1() {"
      "    function h1() {"
      "      function i1() {}"
      "      function i2() {}"
      "    }"
      "    function h2() {"
      "      function i1() {}"
      "      function i2() {}"
      "    }"
      "  }"
      "  function g2() {"
      "    function h1() {"
      "      function i1() {}"
      "      function i2() {}"
      "    }"
      "    function h2() {"
      "      function i1() {}"
      "      function i2() {}"
      "    }"
      "  }"
      "}");
  v8::ScriptCompiler::Source script_source(source);
  i_isolate()->compilation_cache()->DisableScriptAndEval();

  v8::HeapStatistics heap_statistics;
  isolate()->GetHeapStatistics(&heap_statistics);
  size_t peak_mem_after_init = heap_statistics.peak_malloced_memory();
  printf("peak memory after init:          %8zu\n", peak_mem_after_init);

  // Peak memory during lazy compilation should converge to the same value
  // (usually after 1-2 iterations).
  std::vector<size_t> peak_mem_after_lazy_compile;
  const int kNumLazyCompiles = 5;
  for (int i = 0; i < kNumLazyCompiles; i++) {
    v8::ScriptCompiler::Compile(context(), &script_source,
                                v8::ScriptCompiler::kNoCompileOptions)
        .ToLocalChecked();

    isolate()->GetHeapStatistics(&heap_statistics);
    size_t peak_mem = heap_statistics.peak_malloced_memory();
    printf("peak memory after lazy compile:  %8zu\n", peak_mem);
    peak_mem_after_lazy_compile.push_back(peak_mem);
  }
  size_t peak_mem_after_first_lazy_compile = peak_mem_after_lazy_compile[0];
  size_t peak_mem_after_second_to_last_lazy_compile =
      peak_mem_after_lazy_compile[kNumLazyCompiles - 2];
  size_t peak_mem_after_last_lazy_compile =
      peak_mem_after_lazy_compile[kNumLazyCompiles - 1];

  v8::ScriptCompiler::Compile(context(), &script_source,
                              v8::ScriptCompiler::kEagerCompile)
      .ToLocalChecked();

  isolate()->GetHeapStatistics(&heap_statistics);
  size_t peak_mem_after_eager_compile = heap_statistics.peak_malloced_memory();
  printf("peak memory after eager compile: %8zu\n",
         peak_mem_after_eager_compile);

  EXPECT_LE(peak_mem_after_init, peak_mem_after_first_lazy_compile);
  EXPECT_EQ(peak_mem_after_second_to_last_lazy_compile,
            peak_mem_after_last_lazy_compile);
  EXPECT_LE(peak_mem_after_last_lazy_compile, peak_mem_after_eager_compile);
  // Check that eager compilation does not cause significantly higher (+100%)
  // peak memory than lazy compilation.
  EXPECT_LE(peak_mem_after_eager_compile - peak_mem_after_last_lazy_compile,
            peak_mem_after_last_lazy_compile);
}

namespace {

// Dummy external source stream which returns the whole source in one go.
class DummySourceStream : public v8::ScriptCompiler::ExternalSourceStream {
 public:
  explicit DummySourceStream(const char* source) : done_(false) {
    source_length_ = static_cast<int>(strlen(source));
    source_buffer_ = source;
  }

  size_t GetMoreData(const uint8_t** dest) override {
    if (done_) {
      return 0;
    }
    uint8_t* buf = new uint8_t[source_length_ + 1];
    memcpy(buf, source_buffer_, source_length_ + 1);
    *dest = buf;
    done_ = true;
    return source_length_;
  }

 private:
  int source_length_;
  const char* source_buffer_;
  bool done_;
};

}  // namespace

// Tests that doing something that causes source positions to need to be
// collected after a background compilation task has started does result in
// source positions being collected.
TEST_F(CompilerTest, ProfilerEnabledDuringBackgroundCompile) {
  v8::HandleScope scope(isolate());
  const char* source = "var a = 0;";

  v8::ScriptCompiler::StreamedSource streamed_source(
      std::make_unique<DummySourceStream>(source),
      v8::ScriptCompiler::StreamedSource::UTF8);
  std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task(
      v8::ScriptCompiler::StartStreaming(isolate(), &streamed_source));

  // Run the background compilation task. DummySourceStream::GetMoreData won't
  // block, so it's OK to just join the background task.
  StreamerThread::StartThreadForTaskAndJoin(task.get());

  // Enable the CPU profiler.
  auto* cpu_profiler = v8::CpuProfiler::New(isolate(), v8::kStandardNaming);
  v8::Local<v8::String> profile = NewString("profile");
  cpu_profiler->StartProfiling(profile);

  // Finalize the background compilation task ensuring it completed
  // successfully.
  v8::Local<v8::Script> script =
      v8::ScriptCompiler::Compile(isolate()->GetCurrentContext(),
                                  &streamed_source, NewString(source),
                                  v8::ScriptOrigin(NewString("foo")))
          .ToLocalChecked();

  i::DirectHandle<i::Object> obj = Utils::OpenDirectHandle(*script);
  EXPECT_TRUE(
      i::Cast<i::JSFunction>(*obj)->shared()->AreSourcePositionsAvailable(
          i_isolate()));

  cpu_profiler->StopProfiling(profile);
}

using BackgroundMergeTest = TestWithNativeContext;

// Tests that a GC during merge doesn't break the merge.
TEST_F(BackgroundMergeTest, GCDuringMerge) {
  v8_flags.verify_code_merge = true;

  HandleScope scope(isolate());
  const char* source =
      // f is compiled eagerly thanks to the IIFE hack.
      "f = (function f(x) {"
      "  let b = x;"
      // f is compiled eagerly, so g's SFI exists. But, it is not compiled.
      "  return function g() {"
      // g isn't compiled, so h's SFI does not exist.
      "    return function h() {"
      "      return b;"
      "    }"
      "  }"
      "})";
  Handle<String> source_string =
      isolate()
          ->factory()
          ->NewStringFromUtf8(base::CStrVector(source))
          .ToHandleChecked();

  const int kTopLevelId = 0;
  const int kFId = 1;
  const int kGId = 2;
  const int kHId = 3;

  // Compile the script once to warm up the compilation cache.
  Handle<JSFunction> old_g;
  IsCompiledScope old_g_bytecode_keepalive;
  ([&]() V8_NOINLINE {
    // Compile in a new handle scope inside a non-inlined function, so that the
    // script can die while select inner functions stay alive.
    HandleScope scope(isolate());
    ScriptCompiler::CompilationDetails compilation_details;
    Handle<SharedFunctionInfo> top_level_sfi =
        Compiler::GetSharedFunctionInfoForScript(
            isolate(), source_string, ScriptDetails(),
            v8::ScriptCompiler::kNoCompileOptions,
            ScriptCompiler::kNoCacheNoReason, NOT_NATIVES_CODE,
            &compilation_details)
            .ToHandleChecked();

    {
      Tagged<Script> script = Cast<Script>(top_level_sfi->script());
      CHECK(!script->infos()->get(kTopLevelId).IsCleared());
      CHECK(!script->infos()->get(kFId).IsCleared());
      CHECK(!script->infos()->get(kGId).IsCleared());
      // h in the script infos list was never initialized by the compilation, so
      // it's the default value for a WeakFixedArray, which is `undefined`.
      CHECK(Is<Undefined>(script->infos()->get(kHId)));
    }

    Handle<JSFunction> top_level =
        Factory::JSFunctionBuilder{isolate(), top_level_sfi,
                                   isolate()->native_context()}
            .Build();

    Handle<JSObject> global(isolate()->context()->global_object(), isolate());
    Execution::CallScript(isolate(), top_level, global,
                          isolate()->factory()->empty_fixed_array())
        .Check();

    Handle<JSFunction> f = Cast<JSFunction>(
        JSObject::GetProperty(isolate(), global, "f").ToHandleChecked());

    CHECK(f->is_compiled(isolate()));

    // Execute f to get g's SFI (no g bytecode yet)
    Handle<JSFunction> g = Cast<JSFunction>(
        Execution::Call(isolate(), f, global, 0, nullptr).ToHandleChecked());
    CHECK(!g->is_compiled(isolate()));

    // Execute g's SFI to initialize g's bytecode, and to get h.
    Handle<JSFunction> h = Cast<JSFunction>(
        Execution::Call(isolate(), g, global, 0, nullptr).ToHandleChecked());
    CHECK(g->is_compiled(isolate()));
    CHECK(!h->is_compiled(isolate()));

    CHECK_EQ(top_level->shared()->function_literal_id(), kTopLevelId);
    CHECK_EQ(f->shared()->function_literal_id(), kFId);
    CHECK_EQ(g->shared()->function_literal_id(), kGId);
    CHECK_EQ(h->shared()->function_literal_id(), kHId);

    // Age everything so that subsequent GCs can pick it up if possible.
    SharedFunctionInfo::EnsureOldForTesting(top_level->shared());
    SharedFunctionInfo::EnsureOldForTesting(f->shared());
    SharedFunctionInfo::EnsureOldForTesting(g->shared());
    SharedFunctionInfo::EnsureOldForTesting(h->shared());

    old_g = scope.CloseAndEscape(g);
  })();
  Handle<Script> old_script(Cast<Script>(old_g->shared()->script()), isolate());

  // Make sure bytecode is cleared...
  for (int i = 0; i < 3; ++i) {
    InvokeMajorGC();
  }
  CHECK(!old_g->is_compiled(isolate()));

  // The top-level script should now be dead.
  CHECK(old_script->infos()->get(kTopLevelId).IsCleared());
  // f should still be alive by global reference.
  CHECK(!old_script->infos()->get(kFId).IsCleared());
  // g should be kept alive by our old_g handle.
  CHECK(!old_script->infos()->get(kGId).IsCleared());
  // h should be dead since g's bytecode was flushed.
  CHECK(old_script->infos()->get(kHId).IsCleared());

  // Copy the old_script_infos WeakFixedArray, so that we can inspect it after
  // the merge mutated the original.
  Handle<WeakFixedArray> unmutated_old_script_list =
      isolate()->factory()->CopyWeakFixedArray(
          direct_handle(old_script->infos(), isolate()));

  {
    HandleScope scope(isolate());
    ScriptStreamingData streamed_source(
        std::make_unique<DummySourceStream>(source),
        v8::ScriptCompiler::StreamedSource::UTF8);
    ScriptCompiler::CompilationDetails details;
    streamed_source.task = std::make_unique<i::BackgroundCompileTask>(
        &streamed_source, isolate(), ScriptType::kClassic,
        ScriptCompiler::CompileOptions::kNoCompileOptions, &details);

    streamed_source.task->RunOnMainThread(isolate());

    Handle<SharedFunctionInfo> top_level_sfi;
    {
      // Use a manual GC scope, because we want to test a GC in a very precise
      // spot in the merge.
      ManualGCScope manual_gc(isolate());
      // There's one more reference to the old_g -- clear it so that nothing is
      // keeping it alive
      CHECK(!old_script->infos()->get(kGId).IsCleared());
      CHECK(!unmutated_old_script_list->get(kGId).IsCleared());
      old_g.PatchValue({});
      CHECK(!old_script->infos()->get(kFId).IsCleared());

      BackgroundMergeTask::ForceGCDuringNextMergeForTesting();

      top_level_sfi = streamed_source.task
                          ->FinalizeScript(isolate(), source_string,
                                           ScriptDetails(), old_script)
                          .ToHandleChecked();
      CHECK(!old_script->infos()->get(kFId).IsCleared());
    }

    CHECK_EQ(top_level_sfi->script(), *old_script);

    Handle<JSFunction> top_level =
        Factory::JSFunctionBuilder{isolate(), top_level_sfi,
                                   isolate()->native_context()}
            .Build();

    Handle<JSObject> global(isolate()->context()->global_object(), isolate());

    Handle<JSFunction> f = Cast<JSFunction>(
        JSObject::GetProperty(isolate(), global, "f").ToHandleChecked());

    // f should normally be compiled (with the old shared function info but the
    // new bytecode). However, the extra GCs in finalization might cause it to
    // be flushed, so we can't guarantee this check.
    // CHECK(f->is_compiled(isolate()));

    // Execute f to get g's SFI (no g bytecode yet)
    Handle<JSFunction> g = Cast<JSFunction>(
        Execution::Call(isolate(), f, global, 0, nullptr).ToHandleChecked());
    CHECK(!g->is_compiled(isolate()));

    // Execute g's SFI to initialize g's bytecode, and to get h.
    Handle<JSFunction> h = Cast<JSFunction>(
        Execution::Call(isolate(), g, global, 0, nullptr).ToHandleChecked());
    CHECK(g->is_compiled(isolate()));
    CHECK(!h->is_compiled(isolate()));

    CHECK_EQ(top_level->shared()->function_literal_id(), kTopLevelId);
    CHECK_EQ(f->shared()->function_literal_id(), kFId);
    CHECK_EQ(g->shared()->function_literal_id(), kGId);
    CHECK_EQ(h->shared()->function_literal_id(), kHId);

    CHECK_EQ(top_level->shared()->script(), *old_script);
    CHECK_EQ(f->shared()->script(), *old_script);
    CHECK_EQ(g->shared()->script(), *old_script);
    CHECK_EQ(h->shared()->script(), *old_script);

    CHECK_EQ(MakeWeak(top_level->shared()),
             old_script->infos()->get(kTopLevelId));
    CHECK_EQ(MakeWeak(f->shared()), old_script->infos()->get(kFId));
    CHECK_EQ(MakeWeak(g->shared()), old_script->infos()->get(kGId));
    CHECK_EQ(MakeWeak(h->shared()), old_script->infos()->get(kHId));

    // The old top-level died, so we have a new one.
    CHECK_NE(MakeWeak(top_level->shared()),
             unmutated_old_script_list->get(kTopLevelId));
    // The old f was still alive, so it's the same.
    CHECK_EQ(MakeWeak(f->shared()), unmutated_old_script_list->get(kFId));
    // The old g was still alive, so it's the same.
    CHECK_EQ(MakeWeak(g->shared()), unmutated_old_script_list->get(kGId));
    // The old h died, so it's different.
    CHECK_NE(MakeWeak(h->shared()), unmutated_old_script_list->get(kHId));
  }
}

}  // namespace internal
}  // namespace v8
