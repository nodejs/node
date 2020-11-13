// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>
#include <wchar.h>
#include <memory>

#include "src/init/v8.h"

#include "include/v8-profiler.h"
#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/compiler.h"
#include "src/diagnostics/disasm.h"
#include "src/heap/factory.h"
#include "src/heap/spaces.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

static Handle<Object> GetGlobalProperty(const char* name) {
  Isolate* isolate = CcTest::i_isolate();
  return JSReceiver::GetProperty(isolate, isolate->global_object(), name)
      .ToHandleChecked();
}

static void SetGlobalProperty(const char* name, Object value) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<Object> object(value, isolate);
  Handle<String> internalized_name =
      isolate->factory()->InternalizeUtf8String(name);
  Handle<JSObject> global(isolate->context().global_object(), isolate);
  Runtime::SetObjectProperty(isolate, global, internalized_name, object,
                             StoreOrigin::kMaybeKeyed, Just(kDontThrow))
      .Check();
}

static Handle<JSFunction> Compile(const char* source) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<String> source_code = isolate->factory()->NewStringFromUtf8(
      CStrVector(source)).ToHandleChecked();
  Handle<SharedFunctionInfo> shared =
      Compiler::GetSharedFunctionInfoForScript(
          isolate, source_code, Compiler::ScriptDetails(),
          v8::ScriptOriginOptions(), nullptr, nullptr,
          v8::ScriptCompiler::kNoCompileOptions,
          ScriptCompiler::kNoCacheNoReason, NOT_NATIVES_CODE)
          .ToHandleChecked();
  return isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared, isolate->native_context());
}


static double Inc(Isolate* isolate, int x) {
  const char* source = "result = %d + 1;";
  EmbeddedVector<char, 512> buffer;
  SNPrintF(buffer, source, x);

  Handle<JSFunction> fun = Compile(buffer.begin());
  if (fun.is_null()) return -1;

  Handle<JSObject> global(isolate->context().global_object(), isolate);
  Execution::Call(isolate, fun, global, 0, nullptr).Check();
  return GetGlobalProperty("result")->Number();
}


TEST(Inc) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CHECK_EQ(4.0, Inc(CcTest::i_isolate(), 3));
}


static double Add(Isolate* isolate, int x, int y) {
  Handle<JSFunction> fun = Compile("result = x + y;");
  if (fun.is_null()) return -1;

  SetGlobalProperty("x", Smi::FromInt(x));
  SetGlobalProperty("y", Smi::FromInt(y));
  Handle<JSObject> global(isolate->context().global_object(), isolate);
  Execution::Call(isolate, fun, global, 0, nullptr).Check();
  return GetGlobalProperty("result")->Number();
}


TEST(Add) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CHECK_EQ(5.0, Add(CcTest::i_isolate(), 2, 3));
}


static double Abs(Isolate* isolate, int x) {
  Handle<JSFunction> fun = Compile("if (x < 0) result = -x; else result = x;");
  if (fun.is_null()) return -1;

  SetGlobalProperty("x", Smi::FromInt(x));
  Handle<JSObject> global(isolate->context().global_object(), isolate);
  Execution::Call(isolate, fun, global, 0, nullptr).Check();
  return GetGlobalProperty("result")->Number();
}


TEST(Abs) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CHECK_EQ(3.0, Abs(CcTest::i_isolate(), -3));
}


static double Sum(Isolate* isolate, int n) {
  Handle<JSFunction> fun =
      Compile("s = 0; while (n > 0) { s += n; n -= 1; }; result = s;");
  if (fun.is_null()) return -1;

  SetGlobalProperty("n", Smi::FromInt(n));
  Handle<JSObject> global(isolate->context().global_object(), isolate);
  Execution::Call(isolate, fun, global, 0, nullptr).Check();
  return GetGlobalProperty("result")->Number();
}


TEST(Sum) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CHECK_EQ(5050.0, Sum(CcTest::i_isolate(), 100));
}


TEST(Print) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::NewContext({PRINT_EXTENSION_ID});
  v8::Context::Scope context_scope(context);
  const char* source = "for (n = 0; n < 100; ++n) print(n, 1, 2);";
  Handle<JSFunction> fun = Compile(source);
  if (fun.is_null()) return;
  Handle<JSObject> global(CcTest::i_isolate()->context().global_object(),
                          fun->GetIsolate());
  Execution::Call(CcTest::i_isolate(), fun, global, 0, nullptr).Check();
}


// The following test method stems from my coding efforts today. It
// tests all the functionality I have added to the compiler today
TEST(Stuff) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  const char* source =
    "r = 0;\n"
    "a = new Object;\n"
    "if (a == a) r+=1;\n"  // 1
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
    "if (new Cons0().x == 42) r+=64;\n"  // 64
    "if (new Cons0().y == 87) r+=128;\n"  // 128
    "function Cons2(x, y) { this.sum = x + y; }\n"
    "if (new Cons2(3,4).sum == 7) r+=256;";  // 256

  Handle<JSFunction> fun = Compile(source);
  CHECK(!fun.is_null());
  Handle<JSObject> global(CcTest::i_isolate()->context().global_object(),
                          fun->GetIsolate());
  Execution::Call(CcTest::i_isolate(), fun, global, 0, nullptr).Check();
  CHECK_EQ(511.0, GetGlobalProperty("r")->Number());
}


TEST(UncaughtThrow) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  const char* source = "throw 42;";
  Handle<JSFunction> fun = Compile(source);
  CHECK(!fun.is_null());
  Isolate* isolate = fun->GetIsolate();
  Handle<JSObject> global(isolate->context().global_object(), isolate);
  CHECK(Execution::Call(isolate, fun, global, 0, nullptr).is_null());
  CHECK_EQ(42.0, isolate->pending_exception().Number());
}


// Tests calling a builtin function from C/C++ code, and the builtin function
// performs GC. It creates a stack frame looks like following:
//   | C (PerformGC) |
//   |   JS-to-C     |
//   |      JS       |
//   |   C-to-JS     |
TEST(C2JSFrames) {
  FLAG_expose_gc = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context =
      CcTest::NewContext({PRINT_EXTENSION_ID, GC_EXTENSION_ID});
  v8::Context::Scope context_scope(context);

  const char* source = "function foo(a) { gc(), print(a); }";

  Handle<JSFunction> fun0 = Compile(source);
  CHECK(!fun0.is_null());
  Isolate* isolate = fun0->GetIsolate();

  // Run the generated code to populate the global object with 'foo'.
  Handle<JSObject> global(isolate->context().global_object(), isolate);
  Execution::Call(isolate, fun0, global, 0, nullptr).Check();

  Handle<Object> fun1 =
      JSReceiver::GetProperty(isolate, isolate->global_object(), "foo")
          .ToHandleChecked();
  CHECK(fun1->IsJSFunction());

  Handle<Object> argv[] = {
      isolate->factory()->InternalizeString(StaticCharVector("hello"))};
  Execution::Call(isolate,
                  Handle<JSFunction>::cast(fun1),
                  global,
                  arraysize(argv),
                  argv).Check();
}


// Regression 236. Calling InitLineEnds on a Script with undefined
// source resulted in crash.
TEST(Regression236) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  Handle<Script> script = factory->NewScript(factory->empty_string());
  script->set_source(ReadOnlyRoots(CcTest::heap()).undefined_value());
  CHECK_EQ(-1, Script::GetLineNumber(script, 0));
  CHECK_EQ(-1, Script::GetLineNumber(script, 100));
  CHECK_EQ(-1, Script::GetLineNumber(script, -1));
}


TEST(GetScriptLineNumber) {
  LocalContext context;
  v8::HandleScope scope(CcTest::isolate());
  v8::ScriptOrigin origin = v8::ScriptOrigin(v8_str("test"));
  const char function_f[] = "function f() {}";
  const int max_rows = 1000;
  const int buffer_size = max_rows + sizeof(function_f);
  ScopedVector<char> buffer(buffer_size);
  memset(buffer.begin(), '\n', buffer_size - 1);
  buffer[buffer_size - 1] = '\0';

  for (int i = 0; i < max_rows; ++i) {
    if (i > 0)
      buffer[i - 1] = '\n';
    MemCopy(&buffer[i], function_f, sizeof(function_f) - 1);
    v8::Local<v8::String> script_body = v8_str(buffer.begin());
    v8::Script::Compile(context.local(), script_body, &origin)
        .ToLocalChecked()
        ->Run(context.local())
        .ToLocalChecked();
    v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
        context->Global()->Get(context.local(), v8_str("f")).ToLocalChecked());
    CHECK_EQ(i, f->GetScriptLineNumber());
  }
}


TEST(FeedbackVectorPreservedAcrossRecompiles) {
  if (i::FLAG_always_opt || !i::FLAG_opt) return;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer()) return;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function fun() {};"
      "fun1 = fun;"
      "%PrepareFunctionForOptimization(f);"
      "function f(a) { a(); } f(fun1);");

  Handle<JSFunction> f = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(context, v8_str("f")).ToLocalChecked())));

  // Verify that we gathered feedback.
  Handle<FeedbackVector> feedback_vector(f->feedback_vector(), f->GetIsolate());
  CHECK(!feedback_vector->is_empty());
  FeedbackSlot slot_for_a(0);
  MaybeObject object = feedback_vector->Get(slot_for_a);
  {
    HeapObject heap_object;
    CHECK(object->GetHeapObjectIfWeak(&heap_object));
    CHECK(heap_object.IsJSFunction());
  }

  CompileRun("%OptimizeFunctionOnNextCall(f); f(fun1);");

  // Verify that the feedback is still "gathered" despite a recompilation
  // of the full code.
  CHECK(f->HasAttachedOptimizedCode());
  object = f->feedback_vector().Get(slot_for_a);
  {
    HeapObject heap_object;
    CHECK(object->GetHeapObjectIfWeak(&heap_object));
    CHECK(heap_object.IsJSFunction());
  }
}


TEST(FeedbackVectorUnaffectedByScopeChanges) {
  if (i::FLAG_always_opt || !i::FLAG_lazy || i::FLAG_lite_mode) {
    return;
  }
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();

  CompileRun("function builder() {"
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

  Handle<JSFunction> f = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CcTest::global()
                                         ->Get(context, v8_str("morphing_call"))
                                         .ToLocalChecked())));

  // If we are compiling lazily then it should not be compiled, and so no
  // feedback vector allocated yet.
  CHECK(!f->shared().is_compiled());

  CompileRun("morphing_call();");

  // Now a feedback vector / closure feedback cell array is allocated.
  CHECK(f->shared().is_compiled());
  CHECK(f->has_feedback_vector() || f->has_closure_feedback_cell_array());
}

// Test that optimized code for different closures is actually shared.
TEST(OptimizedCodeSharing1) {
  FLAG_stress_compaction = false;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  for (int i = 0; i < 3; i++) {
    LocalContext env;
    env->Global()
        ->Set(env.local(), v8_str("x"), v8::Integer::New(CcTest::isolate(), i))
        .FromJust();
    CompileRun(
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
    Handle<JSFunction> fun1 = Handle<JSFunction>::cast(
        v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
            env->Global()
                ->Get(env.local(), v8_str("closure1"))
                .ToLocalChecked())));
    Handle<JSFunction> fun2 = Handle<JSFunction>::cast(
        v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
            env->Global()
                ->Get(env.local(), v8_str("closure2"))
                .ToLocalChecked())));
    CHECK(fun1->HasAttachedOptimizedCode() ||
          !CcTest::i_isolate()->use_optimizer());
    CHECK(fun2->HasAttachedOptimizedCode() ||
          !CcTest::i_isolate()->use_optimizer());
    CHECK_EQ(fun1->code(), fun2->code());
  }
}

TEST(CompileFunctionInContext) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env;
  CompileRun("var r = 10;");
  v8::Local<v8::Object> math = v8::Local<v8::Object>::Cast(
      env->Global()->Get(env.local(), v8_str("Math")).ToLocalChecked());
  v8::ScriptCompiler::Source script_source(v8_str(
      "a = PI * r * r;"
      "x = r * cos(PI);"
      "y = r * sin(PI / 2);"));
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunctionInContext(env.local(), &script_source,
                                                   0, nullptr, 1, &math)
          .ToLocalChecked();
  CHECK(!fun.IsEmpty());

  i::DisallowCompilation no_compile(CcTest::i_isolate());
  fun->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK(env->Global()->Has(env.local(), v8_str("a")).FromJust());
  v8::Local<v8::Value> a =
      env->Global()->Get(env.local(), v8_str("a")).ToLocalChecked();
  CHECK(a->IsNumber());
  CHECK(env->Global()->Has(env.local(), v8_str("x")).FromJust());
  v8::Local<v8::Value> x =
      env->Global()->Get(env.local(), v8_str("x")).ToLocalChecked();
  CHECK(x->IsNumber());
  CHECK(env->Global()->Has(env.local(), v8_str("y")).FromJust());
  v8::Local<v8::Value> y =
      env->Global()->Get(env.local(), v8_str("y")).ToLocalChecked();
  CHECK(y->IsNumber());
  CHECK_EQ(314.1592653589793, a->NumberValue(env.local()).FromJust());
  CHECK_EQ(-10.0, x->NumberValue(env.local()).FromJust());
  CHECK_EQ(10.0, y->NumberValue(env.local()).FromJust());
}


TEST(CompileFunctionInContextComplex) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env;
  CompileRun(
      "var x = 1;"
      "var y = 2;"
      "var z = 4;"
      "var a = {x: 8, y: 16};"
      "var b = {x: 32};");
  v8::Local<v8::Object> ext[2];
  ext[0] = v8::Local<v8::Object>::Cast(
      env->Global()->Get(env.local(), v8_str("a")).ToLocalChecked());
  ext[1] = v8::Local<v8::Object>::Cast(
      env->Global()->Get(env.local(), v8_str("b")).ToLocalChecked());
  v8::ScriptCompiler::Source script_source(v8_str("result = x + y + z"));
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunctionInContext(env.local(), &script_source,
                                                   0, nullptr, 2, ext)
          .ToLocalChecked();
  CHECK(!fun.IsEmpty());
  fun->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK(env->Global()->Has(env.local(), v8_str("result")).FromJust());
  v8::Local<v8::Value> result =
      env->Global()->Get(env.local(), v8_str("result")).ToLocalChecked();
  CHECK(result->IsNumber());
  CHECK_EQ(52.0, result->NumberValue(env.local()).FromJust());
}


TEST(CompileFunctionInContextArgs) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env;
  CompileRun("var a = {x: 23};");
  v8::Local<v8::Object> ext[1];
  ext[0] = v8::Local<v8::Object>::Cast(
      env->Global()->Get(env.local(), v8_str("a")).ToLocalChecked());
  v8::ScriptCompiler::Source script_source(v8_str("result = x + abc"));
  v8::Local<v8::String> arg = v8_str("abc");
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunctionInContext(env.local(), &script_source,
                                                   1, &arg, 1, ext)
          .ToLocalChecked();
  CHECK_EQ(1, fun->Get(env.local(), v8_str("length"))
                  .ToLocalChecked()
                  ->ToInt32(env.local())
                  .ToLocalChecked()
                  ->Value());
  v8::Local<v8::Value> arg_value = v8::Number::New(CcTest::isolate(), 42.0);
  fun->Call(env.local(), env->Global(), 1, &arg_value).ToLocalChecked();
  CHECK(env->Global()->Has(env.local(), v8_str("result")).FromJust());
  v8::Local<v8::Value> result =
      env->Global()->Get(env.local(), v8_str("result")).ToLocalChecked();
  CHECK(result->IsNumber());
  CHECK_EQ(65.0, result->NumberValue(env.local()).FromJust());
}


TEST(CompileFunctionInContextComments) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env;
  CompileRun("var a = {x: 23, y: 1, z: 2};");
  v8::Local<v8::Object> ext[1];
  ext[0] = v8::Local<v8::Object>::Cast(
      env->Global()->Get(env.local(), v8_str("a")).ToLocalChecked());
  v8::Local<v8::String> source =
      CompileRun("'result = /* y + */ x + a\\u4e00 // + z'").As<v8::String>();
  v8::ScriptCompiler::Source script_source(source);
  v8::Local<v8::String> arg = CompileRun("'a\\u4e00'").As<v8::String>();
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunctionInContext(env.local(), &script_source,
                                                   1, &arg, 1, ext)
          .ToLocalChecked();
  CHECK(!fun.IsEmpty());
  v8::Local<v8::Value> arg_value = v8::Number::New(CcTest::isolate(), 42.0);
  fun->Call(env.local(), env->Global(), 1, &arg_value).ToLocalChecked();
  CHECK(env->Global()->Has(env.local(), v8_str("result")).FromJust());
  v8::Local<v8::Value> result =
      env->Global()->Get(env.local(), v8_str("result")).ToLocalChecked();
  CHECK(result->IsNumber());
  CHECK_EQ(65.0, result->NumberValue(env.local()).FromJust());
}


TEST(CompileFunctionInContextNonIdentifierArgs) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env;
  v8::ScriptCompiler::Source script_source(v8_str("result = 1"));
  v8::Local<v8::String> arg = v8_str("b }");
  CHECK(v8::ScriptCompiler::CompileFunctionInContext(
            env.local(), &script_source, 1, &arg, 0, nullptr)
            .IsEmpty());
}

TEST(CompileFunctionInContextRenderCallSite) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env;
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
    v8::ScriptCompiler::Source script_source(v8_str(source1));
    v8::Local<v8::Function> fun =
        v8::ScriptCompiler::CompileFunctionInContext(
            env.local(), &script_source, 0, nullptr, 0, nullptr)
            .ToLocalChecked();
    CHECK(!fun.IsEmpty());
    v8::Local<v8::Value> result =
        fun->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
    CHECK(result->IsString());
    CHECK(v8::Local<v8::String>::Cast(result)
              ->Equals(env.local(), v8_str(expect1))
              .FromJust());
  }
  {
    v8::ScriptCompiler::Source script_source(v8_str(source2));
    v8::Local<v8::Function> fun =
        v8::ScriptCompiler::CompileFunctionInContext(
            env.local(), &script_source, 0, nullptr, 0, nullptr)
            .ToLocalChecked();
    v8::Local<v8::Value> result =
        fun->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
    CHECK(result->IsString());
    CHECK(v8::Local<v8::String>::Cast(result)
              ->Equals(env.local(), v8_str(expect2))
              .FromJust());
  }
}

TEST(CompileFunctionInContextQuirks) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env;
  {
    static const char* source =
        "[x, y] = ['ab', 'cd'];"
        "return x + y";
    static const char* expect = "abcd";
    v8::ScriptCompiler::Source script_source(v8_str(source));
    v8::Local<v8::Function> fun =
        v8::ScriptCompiler::CompileFunctionInContext(
            env.local(), &script_source, 0, nullptr, 0, nullptr)
            .ToLocalChecked();
    v8::Local<v8::Value> result =
        fun->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
    CHECK(result->IsString());
    CHECK(v8::Local<v8::String>::Cast(result)
              ->Equals(env.local(), v8_str(expect))
              .FromJust());
  }
  {
    static const char* source = "'use strict'; var a = 077";
    v8::ScriptCompiler::Source script_source(v8_str(source));
    v8::TryCatch try_catch(CcTest::isolate());
    CHECK(v8::ScriptCompiler::CompileFunctionInContext(
              env.local(), &script_source, 0, nullptr, 0, nullptr)
              .IsEmpty());
    CHECK(try_catch.HasCaught());
  }
  {
    static const char* source = "{ let x; { var x } }";
    v8::ScriptCompiler::Source script_source(v8_str(source));
    v8::TryCatch try_catch(CcTest::isolate());
    CHECK(v8::ScriptCompiler::CompileFunctionInContext(
              env.local(), &script_source, 0, nullptr, 0, nullptr)
              .IsEmpty());
    CHECK(try_catch.HasCaught());
  }
}

TEST(CompileFunctionInContextScriptOrigin) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env;
  v8::ScriptOrigin origin(v8_str("test"),
                          v8::Integer::New(CcTest::isolate(), 22),
                          v8::Integer::New(CcTest::isolate(), 41));
  v8::ScriptCompiler::Source script_source(v8_str("throw new Error()"), origin);
  Local<ScriptOrModule> script;
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunctionInContext(
          env.local(), &script_source, 0, nullptr, 0, nullptr,
          v8::ScriptCompiler::CompileOptions::kNoCompileOptions,
          v8::ScriptCompiler::NoCacheReason::kNoCacheNoReason, &script)
          .ToLocalChecked();
  CHECK(!fun.IsEmpty());
  CHECK(!script.IsEmpty());
  CHECK(script->GetResourceName()->StrictEquals(v8_str("test")));
  v8::TryCatch try_catch(CcTest::isolate());
  CcTest::isolate()->SetCaptureStackTraceForUncaughtExceptions(true);
  CHECK(fun->Call(env.local(), env->Global(), 0, nullptr).IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(!try_catch.Exception().IsEmpty());
  v8::Local<v8::StackTrace> stack =
      v8::Exception::GetStackTrace(try_catch.Exception());
  CHECK(!stack.IsEmpty());
  CHECK_GT(stack->GetFrameCount(), 0);
  v8::Local<v8::StackFrame> frame = stack->GetFrame(CcTest::isolate(), 0);
  CHECK_EQ(23, frame->GetLineNumber());
  CHECK_EQ(42 + strlen("throw "), static_cast<unsigned>(frame->GetColumn()));
}

void TestCompileFunctionInContextToStringImpl() {
#define CHECK_NOT_CAUGHT(__local_context__, try_catch, __op__)             \
  do {                                                                     \
    const char* op = (__op__);                                             \
    v8::Local<v8::Context> context = (__local_context__);                  \
    if (try_catch.HasCaught()) {                                           \
      v8::String::Utf8Value error(                                         \
          CcTest::isolate(),                                               \
          try_catch.Exception()->ToString(context).ToLocalChecked());      \
      FATAL("Unexpected exception thrown during %s:\n\t%s\n", op, *error); \
    }                                                                      \
  } while (false)

  {  // NOLINT
    CcTest::InitializeVM();
    v8::HandleScope scope(CcTest::isolate());
    LocalContext env;

    // Regression test for v8:6190
    {
      v8::ScriptOrigin origin(v8_str("test"), v8_int(22), v8_int(41));
      v8::ScriptCompiler::Source script_source(v8_str("return event"), origin);

      v8::Local<v8::String> params[] = {v8_str("event")};
      v8::TryCatch try_catch(CcTest::isolate());
      v8::MaybeLocal<v8::Function> maybe_fun =
          v8::ScriptCompiler::CompileFunctionInContext(
              env.local(), &script_source, arraysize(params), params, 0,
              nullptr);

      CHECK_NOT_CAUGHT(env.local(), try_catch,
                       "v8::ScriptCompiler::CompileFunctionInContext");

      v8::Local<v8::Function> fun = maybe_fun.ToLocalChecked();
      CHECK(!fun.IsEmpty());
      CHECK(!try_catch.HasCaught());
      v8::Local<v8::String> result =
          fun->ToString(env.local()).ToLocalChecked();
      v8::Local<v8::String> expected = v8_str(
          "function (event) {\n"
          "return event\n"
          "}");
      CHECK(expected->Equals(env.local(), result).FromJust());
    }

    // With no parameters:
    {
      v8::ScriptOrigin origin(v8_str("test"), v8_int(17), v8_int(31));
      v8::ScriptCompiler::Source script_source(v8_str("return 0"), origin);

      v8::TryCatch try_catch(CcTest::isolate());
      v8::MaybeLocal<v8::Function> maybe_fun =
          v8::ScriptCompiler::CompileFunctionInContext(
              env.local(), &script_source, 0, nullptr, 0, nullptr);

      CHECK_NOT_CAUGHT(env.local(), try_catch,
                       "v8::ScriptCompiler::CompileFunctionInContext");

      v8::Local<v8::Function> fun = maybe_fun.ToLocalChecked();
      CHECK(!fun.IsEmpty());
      CHECK(!try_catch.HasCaught());
      v8::Local<v8::String> result =
          fun->ToString(env.local()).ToLocalChecked();
      v8::Local<v8::String> expected = v8_str(
          "function () {\n"
          "return 0\n"
          "}");
      CHECK(expected->Equals(env.local(), result).FromJust());
    }

    // With a name:
    {
      v8::ScriptOrigin origin(v8_str("test"), v8_int(17), v8_int(31));
      v8::ScriptCompiler::Source script_source(v8_str("return 0"), origin);

      v8::TryCatch try_catch(CcTest::isolate());
      v8::MaybeLocal<v8::Function> maybe_fun =
          v8::ScriptCompiler::CompileFunctionInContext(
              env.local(), &script_source, 0, nullptr, 0, nullptr);

      CHECK_NOT_CAUGHT(env.local(), try_catch,
                       "v8::ScriptCompiler::CompileFunctionInContext");

      v8::Local<v8::Function> fun = maybe_fun.ToLocalChecked();
      CHECK(!fun.IsEmpty());
      CHECK(!try_catch.HasCaught());

      fun->SetName(v8_str("onclick"));

      v8::Local<v8::String> result =
          fun->ToString(env.local()).ToLocalChecked();
      v8::Local<v8::String> expected = v8_str(
          "function onclick() {\n"
          "return 0\n"
          "}");
      CHECK(expected->Equals(env.local(), result).FromJust());
    }
  }
#undef CHECK_NOT_CAUGHT
}

TEST(CompileFunctionInContextFunctionToString) {
  TestCompileFunctionInContextToStringImpl();
}

TEST(InvocationCount) {
  if (FLAG_lite_mode) return;
  FLAG_allow_natives_syntax = true;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "function bar() {};"
      "%EnsureFeedbackVectorForFunction(bar);"
      "function foo() { return bar(); };"
      "%EnsureFeedbackVectorForFunction(foo);"
      "foo();");
  Handle<JSFunction> foo = Handle<JSFunction>::cast(GetGlobalProperty("foo"));
  CHECK_EQ(1, foo->feedback_vector().invocation_count());
  CompileRun("foo()");
  CHECK_EQ(2, foo->feedback_vector().invocation_count());
  CompileRun("bar()");
  CHECK_EQ(2, foo->feedback_vector().invocation_count());
  CompileRun("foo(); foo()");
  CHECK_EQ(4, foo->feedback_vector().invocation_count());
}

TEST(SafeToSkipArgumentsAdaptor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CompileRun(
      "function a() { \"use strict\"; }; a();"
      "function b() { }; b();"
      "function c() { \"use strict\"; return arguments; }; c();"
      "function d(...args) { return args; }; d();"
      "function e() { \"use strict\"; return eval(\"\"); }; e();"
      "function f(x, y) { \"use strict\"; return x + y; }; f(1, 2);");
  Handle<JSFunction> a = Handle<JSFunction>::cast(GetGlobalProperty("a"));
  CHECK(a->shared().is_safe_to_skip_arguments_adaptor());
  Handle<JSFunction> b = Handle<JSFunction>::cast(GetGlobalProperty("b"));
  CHECK(!b->shared().is_safe_to_skip_arguments_adaptor());
  Handle<JSFunction> c = Handle<JSFunction>::cast(GetGlobalProperty("c"));
  CHECK(!c->shared().is_safe_to_skip_arguments_adaptor());
  Handle<JSFunction> d = Handle<JSFunction>::cast(GetGlobalProperty("d"));
  CHECK(!d->shared().is_safe_to_skip_arguments_adaptor());
  Handle<JSFunction> e = Handle<JSFunction>::cast(GetGlobalProperty("e"));
  CHECK(!e->shared().is_safe_to_skip_arguments_adaptor());
  Handle<JSFunction> f = Handle<JSFunction>::cast(GetGlobalProperty("f"));
  CHECK(f->shared().is_safe_to_skip_arguments_adaptor());
}

TEST(ShallowEagerCompilation) {
  i::FLAG_always_opt = false;
  CcTest::InitializeVM();
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::String> source = v8_str(
      "function f(x) {"
      "  return x + x;"
      "}"
      "f(2)");
  v8::ScriptCompiler::Source script_source(source);
  v8::Local<v8::Script> script =
      v8::ScriptCompiler::Compile(env.local(), &script_source,
                                  v8::ScriptCompiler::kEagerCompile)
          .ToLocalChecked();
  {
    v8::internal::DisallowCompilation no_compile_expected(isolate);
    v8::Local<v8::Value> result = script->Run(env.local()).ToLocalChecked();
    CHECK_EQ(4, result->Int32Value(env.local()).FromJust());
  }
}

TEST(DeepEagerCompilation) {
  i::FLAG_always_opt = false;
  CcTest::InitializeVM();
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::String> source = v8_str(
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
      v8::ScriptCompiler::Compile(env.local(), &script_source,
                                  v8::ScriptCompiler::kEagerCompile)
          .ToLocalChecked();
  {
    v8::internal::DisallowCompilation no_compile_expected(isolate);
    v8::Local<v8::Value> result = script->Run(env.local()).ToLocalChecked();
    CHECK_EQ(32, result->Int32Value(env.local()).FromJust());
  }
}

TEST(DeepEagerCompilationPeakMemory) {
  i::FLAG_always_opt = false;
  CcTest::InitializeVM();
  LocalContext env;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::String> source = v8_str(
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
  CcTest::i_isolate()->compilation_cache()->DisableScriptAndEval();

  v8::HeapStatistics heap_statistics;
  CcTest::isolate()->GetHeapStatistics(&heap_statistics);
  size_t peak_mem_1 = heap_statistics.peak_malloced_memory();
  printf("peak memory after init:          %8zu\n", peak_mem_1);

  v8::ScriptCompiler::Compile(env.local(), &script_source,
                              v8::ScriptCompiler::kNoCompileOptions)
      .ToLocalChecked();

  CcTest::isolate()->GetHeapStatistics(&heap_statistics);
  size_t peak_mem_2 = heap_statistics.peak_malloced_memory();
  printf("peak memory after lazy compile:  %8zu\n", peak_mem_2);

  v8::ScriptCompiler::Compile(env.local(), &script_source,
                              v8::ScriptCompiler::kNoCompileOptions)
      .ToLocalChecked();

  CcTest::isolate()->GetHeapStatistics(&heap_statistics);
  size_t peak_mem_3 = heap_statistics.peak_malloced_memory();
  printf("peak memory after lazy compile:  %8zu\n", peak_mem_3);

  v8::ScriptCompiler::Compile(env.local(), &script_source,
                              v8::ScriptCompiler::kEagerCompile)
      .ToLocalChecked();

  CcTest::isolate()->GetHeapStatistics(&heap_statistics);
  size_t peak_mem_4 = heap_statistics.peak_malloced_memory();
  printf("peak memory after eager compile: %8zu\n", peak_mem_4);

  CHECK_LE(peak_mem_1, peak_mem_2);
  CHECK_EQ(peak_mem_2, peak_mem_3);
  CHECK_LE(peak_mem_3, peak_mem_4);
  // Check that eager compilation does not cause significantly higher (+100%)
  // peak memory than lazy compilation.
  CHECK_LE(peak_mem_4 - peak_mem_3, peak_mem_3);
}

// TODO(mslekova): Remove the duplication with test-heap.cc
static int AllocationSitesCount(Heap* heap) {
  int count = 0;
  for (Object site = heap->allocation_sites_list(); site.IsAllocationSite();) {
    AllocationSite cur = AllocationSite::cast(site);
    CHECK(cur.HasWeakNext());
    site = cur.weak_next();
    count++;
  }
  return count;
}

// This test simulates a specific race-condition if GC is triggered just
// before CompilationDependencies::Commit is finished, and this changes
// the pretenuring decision, thus causing a deoptimization.
TEST(DecideToPretenureDuringCompilation) {
  // The test makes use of optimization and relies on deterministic
  // compilation.
  if (!i::FLAG_opt || i::FLAG_always_opt || i::FLAG_minor_mc ||
      i::FLAG_stress_incremental_marking || i::FLAG_optimize_for_size ||
      i::FLAG_turbo_nci || i::FLAG_turbo_nci_as_midtier ||
      i::FLAG_stress_concurrent_allocation) {
    return;
  }

  FLAG_stress_gc_during_compilation = true;
  FLAG_allow_natives_syntax = true;
  FLAG_allocation_site_pretenuring = true;
  FLAG_flush_bytecode = false;

  // We want to trigger exactly 1 optimization.
  FLAG_use_osr = false;

  // We'll do manual initialization.
  ManualGCScope manual_gc_scope;
  v8::Isolate::CreateParams create_params;

  // This setting ensures Heap::MaximumSizeScavenge will return `true`.
  // We need to initialize the heap with at least 1 page, while keeping the
  // limit low, to ensure the new space fills even on 32-bit architectures.
  create_params.constraints.set_max_young_generation_size_in_bytes(
      3 * Page::kPageSize);
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  isolate->Enter();
  {
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    Heap* heap = i_isolate->heap();
    GlobalHandles* global_handles = i_isolate->global_handles();
    HandleScope handle_scope(i_isolate);

    // The allocation site at the head of the list is ours.
    Handle<AllocationSite> site;
    {
      LocalContext context(isolate);
      v8::HandleScope scope(context->GetIsolate());

      int count = AllocationSitesCount(heap);
      CompileRun(
          "let arr = [];"
          "function foo(shouldKeep) {"
          "  let local_array = new Array();"
          "  if (shouldKeep) arr.push(local_array);"
          "}"
          "function bar(shouldKeep) {"
          "  for (let i = 0; i < 10000; i++) {"
          "    foo(shouldKeep);"
          "  }"
          "}"
          "%PrepareFunctionForOptimization(bar);"
          "bar();");

      // This number should be >= kPretenureRatio * 10000,
      // where 10000 is the number of iterations in `bar`,
      // in order to make the ratio in DigestPretenuringFeedback close to 1.
      const int memento_found_bump = 8500;

      // One allocation site should have been created.
      int new_count = AllocationSitesCount(heap);
      CHECK_EQ(new_count, (count + 1));
      site = Handle<AllocationSite>::cast(global_handles->Create(
          AllocationSite::cast(heap->allocation_sites_list())));
      site->set_memento_found_count(memento_found_bump);

      CompileRun("%OptimizeFunctionOnNextCall(bar);");
      CompileRun("bar(true);");

      // The last call should have caused `foo` to bail out of compilation
      // due to dependency change (the pretenuring decision in this case).
      // This will cause recompilation.

      // Check `bar` can get optimized again, meaning the compiler state is
      // recoverable from this point.
      CompileRun(
          "%PrepareFunctionForOptimization(bar);"
          "%OptimizeFunctionOnNextCall(bar);");
      CompileRun("bar();");

      Handle<Object> foo_obj =
          JSReceiver::GetProperty(i_isolate, i_isolate->global_object(), "bar")
              .ToHandleChecked();
      Handle<JSFunction> bar = Handle<JSFunction>::cast(foo_obj);

      CHECK(bar->HasAttachedOptimizedCode());
    }
  }
  isolate->Exit();
  isolate->Dispose();
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
TEST(ProfilerEnabledDuringBackgroundCompile) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  const char* source = "var a = 0;";

  v8::ScriptCompiler::StreamedSource streamed_source(
      std::make_unique<DummySourceStream>(source),
      v8::ScriptCompiler::StreamedSource::UTF8);
  std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task(
      v8::ScriptCompiler::StartStreamingScript(isolate, &streamed_source));

  // Run the background compilation task on the main thread.
  task->Run();

  // Enable the CPU profiler.
  auto* cpu_profiler = v8::CpuProfiler::New(isolate, v8::kStandardNaming);
  v8::Local<v8::String> profile = v8_str("profile");
  cpu_profiler->StartProfiling(profile);

  // Finalize the background compilation task ensuring it completed
  // successfully.
  v8::Local<v8::Script> script =
      v8::ScriptCompiler::Compile(isolate->GetCurrentContext(),
                                  &streamed_source, v8_str(source),
                                  v8::ScriptOrigin(v8_str("foo")))
          .ToLocalChecked();

  i::Handle<i::Object> obj = Utils::OpenHandle(*script);
  CHECK(i::JSFunction::cast(*obj).shared().AreSourcePositionsAvailable());

  cpu_profiler->StopProfiling(profile);
}

}  // namespace internal
}  // namespace v8
