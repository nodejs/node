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

#include "src/v8.h"

#include "src/api.h"
#include "src/compiler.h"
#include "src/disasm.h"
#include "src/factory.h"
#include "src/interpreter/interpreter.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;

static Handle<Object> GetGlobalProperty(const char* name) {
  Isolate* isolate = CcTest::i_isolate();
  return JSReceiver::GetProperty(isolate, isolate->global_object(), name)
      .ToHandleChecked();
}


static void SetGlobalProperty(const char* name, Object* value) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<Object> object(value, isolate);
  Handle<String> internalized_name =
      isolate->factory()->InternalizeUtf8String(name);
  Handle<JSObject> global(isolate->context()->global_object());
  Runtime::SetObjectProperty(isolate, global, internalized_name, object,
                             SLOPPY).Check();
}


static Handle<JSFunction> Compile(const char* source) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<String> source_code = isolate->factory()->NewStringFromUtf8(
      CStrVector(source)).ToHandleChecked();
  Handle<SharedFunctionInfo> shared = Compiler::GetSharedFunctionInfoForScript(
      source_code, Handle<String>(), 0, 0, v8::ScriptOriginOptions(),
      Handle<Object>(), Handle<Context>(isolate->native_context()), NULL, NULL,
      v8::ScriptCompiler::kNoCompileOptions, NOT_NATIVES_CODE, false);
  return isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared, isolate->native_context());
}


static double Inc(Isolate* isolate, int x) {
  const char* source = "result = %d + 1;";
  EmbeddedVector<char, 512> buffer;
  SNPrintF(buffer, source, x);

  Handle<JSFunction> fun = Compile(buffer.start());
  if (fun.is_null()) return -1;

  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(isolate, fun, global, 0, NULL).Check();
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
  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(isolate, fun, global, 0, NULL).Check();
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
  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(isolate, fun, global, 0, NULL).Check();
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
  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(isolate, fun, global, 0, NULL).Check();
  return GetGlobalProperty("result")->Number();
}


TEST(Sum) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CHECK_EQ(5050.0, Sum(CcTest::i_isolate(), 100));
}


TEST(Print) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::NewContext(PRINT_EXTENSION);
  v8::Context::Scope context_scope(context);
  const char* source = "for (n = 0; n < 100; ++n) print(n, 1, 2);";
  Handle<JSFunction> fun = Compile(source);
  if (fun.is_null()) return;
  Handle<JSObject> global(CcTest::i_isolate()->context()->global_object());
  Execution::Call(CcTest::i_isolate(), fun, global, 0, NULL).Check();
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
  Handle<JSObject> global(CcTest::i_isolate()->context()->global_object());
  Execution::Call(
      CcTest::i_isolate(), fun, global, 0, NULL).Check();
  CHECK_EQ(511.0, GetGlobalProperty("r")->Number());
}


TEST(UncaughtThrow) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  const char* source = "throw 42;";
  Handle<JSFunction> fun = Compile(source);
  CHECK(!fun.is_null());
  Isolate* isolate = fun->GetIsolate();
  Handle<JSObject> global(isolate->context()->global_object());
  CHECK(Execution::Call(isolate, fun, global, 0, NULL).is_null());
  CHECK_EQ(42.0, isolate->pending_exception()->Number());
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
    CcTest::NewContext(PRINT_EXTENSION | GC_EXTENSION);
  v8::Context::Scope context_scope(context);

  const char* source = "function foo(a) { gc(), print(a); }";

  Handle<JSFunction> fun0 = Compile(source);
  CHECK(!fun0.is_null());
  Isolate* isolate = fun0->GetIsolate();

  // Run the generated code to populate the global object with 'foo'.
  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(isolate, fun0, global, 0, NULL).Check();

  Handle<Object> fun1 =
      JSReceiver::GetProperty(isolate, isolate->global_object(), "foo")
          .ToHandleChecked();
  CHECK(fun1->IsJSFunction());

  Handle<Object> argv[] = {isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("hello"))};
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
  script->set_source(CcTest::heap()->undefined_value());
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
  memset(buffer.start(), '\n', buffer_size - 1);
  buffer[buffer_size - 1] = '\0';

  for (int i = 0; i < max_rows; ++i) {
    if (i > 0)
      buffer[i - 1] = '\n';
    MemCopy(&buffer[i], function_f, sizeof(function_f) - 1);
    v8::Local<v8::String> script_body = v8_str(buffer.start());
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
  if (i::FLAG_always_opt || !i::FLAG_crankshaft) return;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft()) return;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun("function fun() {};"
             "fun1 = fun;"
             "function f(a) { a(); } f(fun1);");

  Handle<JSFunction> f = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(context, v8_str("f")).ToLocalChecked())));

  // We shouldn't have deoptimization support. We want to recompile and
  // verify that our feedback vector preserves information.
  CHECK(!f->shared()->has_deoptimization_support());
  Handle<TypeFeedbackVector> feedback_vector(f->feedback_vector());

  // Verify that we gathered feedback.
  CHECK(!feedback_vector->is_empty());
  FeedbackVectorSlot slot_for_a(0);
  Object* object = feedback_vector->Get(slot_for_a);
  CHECK(object->IsWeakCell() &&
        WeakCell::cast(object)->value()->IsJSFunction());

  CompileRun("%OptimizeFunctionOnNextCall(f); f(fun1);");

  // Verify that the feedback is still "gathered" despite a recompilation
  // of the full code.
  CHECK(f->IsOptimized());
  // If the baseline code is bytecode, then it will not have deoptimization
  // support. has_deoptimization_support() check is only required if the
  // baseline code is from fullcodegen.
  CHECK(f->shared()->has_deoptimization_support() || i::FLAG_ignition);
  object = f->feedback_vector()->Get(slot_for_a);
  CHECK(object->IsWeakCell() &&
        WeakCell::cast(object)->value()->IsJSFunction());
}


TEST(FeedbackVectorUnaffectedByScopeChanges) {
  if (i::FLAG_always_opt || !i::FLAG_lazy ||
      (FLAG_ignition && FLAG_ignition_eager)) {
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
  CHECK(!f->shared()->is_compiled());
  CHECK(f->feedback_vector()->is_empty());

  CompileRun("morphing_call();");

  // Now a feedback vector is allocated.
  CHECK(f->shared()->is_compiled());
  CHECK(!f->feedback_vector()->is_empty());
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
        "%DebugPrint(closure0());"
        "%OptimizeFunctionOnNextCall(closure0);"
        "%DebugPrint(closure0());"
        "var closure1 = MakeClosure(); closure1();"
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
    CHECK(fun1->IsOptimized() || !CcTest::i_isolate()->use_crankshaft());
    CHECK(fun2->IsOptimized() || !CcTest::i_isolate()->use_crankshaft());
    CHECK_EQ(fun1->code(), fun2->code());
  }
}

// Test that optimized code for different closures is actually shared.
TEST(OptimizedCodeSharing2) {
  if (FLAG_stress_compaction) return;
  FLAG_allow_natives_syntax = true;
  FLAG_native_context_specialization = false;
  FLAG_turbo_cache_shared_code = true;
  const char* flag = "--turbo-filter=*";
  FlagList::SetFlagsFromString(flag, StrLength(flag));
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Script> script = v8_compile(
      "function MakeClosure() {"
      "  return function() { return x; };"
      "}");
  Handle<Code> reference_code;
  {
    LocalContext env;
    env->Global()
        ->Set(env.local(), v8_str("x"), v8::Integer::New(CcTest::isolate(), 23))
        .FromJust();
    script->GetUnboundScript()
        ->BindToCurrentContext()
        ->Run(env.local())
        .ToLocalChecked();
    CompileRun(
        "var closure0 = MakeClosure();"
        "%DebugPrint(closure0());"
        "%OptimizeFunctionOnNextCall(closure0);"
        "%DebugPrint(closure0());");
    Handle<JSFunction> fun0 = Handle<JSFunction>::cast(
        v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
            env->Global()
                ->Get(env.local(), v8_str("closure0"))
                .ToLocalChecked())));
    CHECK(fun0->IsOptimized() || !CcTest::i_isolate()->use_crankshaft());
    reference_code = handle(fun0->code());
  }
  for (int i = 0; i < 3; i++) {
    LocalContext env;
    env->Global()
        ->Set(env.local(), v8_str("x"), v8::Integer::New(CcTest::isolate(), i))
        .FromJust();
    script->GetUnboundScript()
        ->BindToCurrentContext()
        ->Run(env.local())
        .ToLocalChecked();
    CompileRun(
        "var closure0 = MakeClosure();"
        "%DebugPrint(closure0());"
        "%OptimizeFunctionOnNextCall(closure0);"
        "%DebugPrint(closure0());"
        "var closure1 = MakeClosure(); closure1();"
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
    CHECK(fun1->IsOptimized() || !CcTest::i_isolate()->use_crankshaft());
    CHECK(fun2->IsOptimized() || !CcTest::i_isolate()->use_crankshaft());
    CHECK_EQ(*reference_code, fun1->code());
    CHECK_EQ(*reference_code, fun2->code());
  }
}

// Test that optimized code for different closures is actually shared.
TEST(OptimizedCodeSharing3) {
  if (FLAG_stress_compaction) return;
  FLAG_allow_natives_syntax = true;
  FLAG_native_context_specialization = false;
  FLAG_turbo_cache_shared_code = true;
  const char* flag = "--turbo-filter=*";
  FlagList::SetFlagsFromString(flag, StrLength(flag));
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Script> script = v8_compile(
      "function MakeClosure() {"
      "  return function() { return x; };"
      "}");
  Handle<Code> reference_code;
  {
    LocalContext env;
    env->Global()
        ->Set(env.local(), v8_str("x"), v8::Integer::New(CcTest::isolate(), 23))
        .FromJust();
    script->GetUnboundScript()
        ->BindToCurrentContext()
        ->Run(env.local())
        .ToLocalChecked();
    CompileRun(
        "var closure0 = MakeClosure();"
        "%DebugPrint(closure0());"
        "%OptimizeFunctionOnNextCall(closure0);"
        "%DebugPrint(closure0());");
    Handle<JSFunction> fun0 = Handle<JSFunction>::cast(
        v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
            env->Global()
                ->Get(env.local(), v8_str("closure0"))
                .ToLocalChecked())));
    CHECK(fun0->IsOptimized() || !CcTest::i_isolate()->use_crankshaft());
    reference_code = handle(fun0->code());
    // Evict only the context-dependent entry from the optimized code map. This
    // leaves it in a state where only the context-independent entry exists.
    fun0->shared()->TrimOptimizedCodeMap(SharedFunctionInfo::kEntryLength);
  }
  for (int i = 0; i < 3; i++) {
    LocalContext env;
    env->Global()
        ->Set(env.local(), v8_str("x"), v8::Integer::New(CcTest::isolate(), i))
        .FromJust();
    script->GetUnboundScript()
        ->BindToCurrentContext()
        ->Run(env.local())
        .ToLocalChecked();
    CompileRun(
        "var closure0 = MakeClosure();"
        "%DebugPrint(closure0());"
        "%OptimizeFunctionOnNextCall(closure0);"
        "%DebugPrint(closure0());"
        "var closure1 = MakeClosure(); closure1();"
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
    CHECK(fun1->IsOptimized() || !CcTest::i_isolate()->use_crankshaft());
    CHECK(fun2->IsOptimized() || !CcTest::i_isolate()->use_crankshaft());
    CHECK_EQ(*reference_code, fun1->code());
    CHECK_EQ(*reference_code, fun2->code());
  }
}


TEST(CompileFunctionInContext) {
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
                                                   0, NULL, 1, &math)
          .ToLocalChecked();
  CHECK(!fun.IsEmpty());
  fun->Call(env.local(), env->Global(), 0, NULL).ToLocalChecked();
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
                                                   0, NULL, 2, ext)
          .ToLocalChecked();
  CHECK(!fun.IsEmpty());
  fun->Call(env.local(), env->Global(), 0, NULL).ToLocalChecked();
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
  v8::ScriptCompiler::Source script_source(v8_str("result = x + b"));
  v8::Local<v8::String> arg = v8_str("b");
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunctionInContext(env.local(), &script_source,
                                                   1, &arg, 1, ext)
          .ToLocalChecked();
  CHECK(!fun.IsEmpty());
  v8::Local<v8::Value> b_value = v8::Number::New(CcTest::isolate(), 42.0);
  fun->Call(env.local(), env->Global(), 1, &b_value).ToLocalChecked();
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
  v8::ScriptCompiler::Source script_source(
      v8_str("result = /* y + */ x + b // + z"));
  v8::Local<v8::String> arg = v8_str("b");
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunctionInContext(env.local(), &script_source,
                                                   1, &arg, 1, ext)
          .ToLocalChecked();
  CHECK(!fun.IsEmpty());
  v8::Local<v8::Value> b_value = v8::Number::New(CcTest::isolate(), 42.0);
  fun->Call(env.local(), env->Global(), 1, &b_value).ToLocalChecked();
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
            env.local(), &script_source, 1, &arg, 0, NULL)
            .IsEmpty());
}


TEST(CompileFunctionInContextScriptOrigin) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env;
  v8::ScriptOrigin origin(v8_str("test"),
                          v8::Integer::New(CcTest::isolate(), 22),
                          v8::Integer::New(CcTest::isolate(), 41));
  v8::ScriptCompiler::Source script_source(v8_str("throw new Error()"), origin);
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunctionInContext(env.local(), &script_source,
                                                   0, NULL, 0, NULL)
          .ToLocalChecked();
  CHECK(!fun.IsEmpty());
  v8::TryCatch try_catch(CcTest::isolate());
  CcTest::isolate()->SetCaptureStackTraceForUncaughtExceptions(true);
  CHECK(fun->Call(env.local(), env->Global(), 0, NULL).IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(!try_catch.Exception().IsEmpty());
  v8::Local<v8::StackTrace> stack =
      v8::Exception::GetStackTrace(try_catch.Exception());
  CHECK(!stack.IsEmpty());
  CHECK(stack->GetFrameCount() > 0);
  v8::Local<v8::StackFrame> frame = stack->GetFrame(0);
  CHECK_EQ(23, frame->GetLineNumber());
  CHECK_EQ(42 + strlen("throw "), static_cast<unsigned>(frame->GetColumn()));
}


#ifdef ENABLE_DISASSEMBLER
static Handle<JSFunction> GetJSFunction(v8::Local<v8::Object> obj,
                                        const char* property_name) {
  v8::Local<v8::Function> fun = v8::Local<v8::Function>::Cast(
      obj->Get(CcTest::isolate()->GetCurrentContext(), v8_str(property_name))
          .ToLocalChecked());
  return Handle<JSFunction>::cast(v8::Utils::OpenHandle(*fun));
}


static void CheckCodeForUnsafeLiteral(Handle<JSFunction> f) {
  // Create a disassembler with default name lookup.
  disasm::NameConverter name_converter;
  disasm::Disassembler d(name_converter);

  if (f->code()->kind() == Code::FUNCTION) {
    Address pc = f->code()->instruction_start();
    int decode_size =
        Min(f->code()->instruction_size(),
            static_cast<int>(f->code()->back_edge_table_offset()));
    if (FLAG_enable_embedded_constant_pool) {
      decode_size = Min(decode_size, f->code()->constant_pool_offset());
    }
    Address end = pc + decode_size;

    v8::internal::EmbeddedVector<char, 128> decode_buffer;
    v8::internal::EmbeddedVector<char, 128> smi_hex_buffer;
    Smi* smi = Smi::FromInt(12345678);
    SNPrintF(smi_hex_buffer, "0x%" V8PRIxPTR, reinterpret_cast<intptr_t>(smi));
    while (pc < end) {
      int num_const = d.ConstantPoolSizeAt(pc);
      if (num_const >= 0) {
        pc += (num_const + 1) * kPointerSize;
      } else {
        pc += d.InstructionDecode(decode_buffer, pc);
        CHECK(strstr(decode_buffer.start(), smi_hex_buffer.start()) == NULL);
      }
    }
  }
}


TEST(SplitConstantsInFullCompiler) {
  LocalContext context;
  v8::HandleScope scope(CcTest::isolate());

  CompileRun("function f() { a = 12345678 }; f();");
  CheckCodeForUnsafeLiteral(GetJSFunction(context->Global(), "f"));
  CompileRun("function f(x) { a = 12345678 + x}; f(1);");
  CheckCodeForUnsafeLiteral(GetJSFunction(context->Global(), "f"));
  CompileRun("function f(x) { var arguments = 1; x += 12345678}; f(1);");
  CheckCodeForUnsafeLiteral(GetJSFunction(context->Global(), "f"));
  CompileRun("function f(x) { var arguments = 1; x = 12345678}; f(1);");
  CheckCodeForUnsafeLiteral(GetJSFunction(context->Global(), "f"));
}
#endif

static void IsBaselineCompiled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Handle<Object> object = v8::Utils::OpenHandle(*args[0]);
  Handle<JSFunction> function = Handle<JSFunction>::cast(object);
  bool is_baseline = function->shared()->code()->kind() == Code::FUNCTION;
  return args.GetReturnValue().Set(is_baseline);
}

static void InstallIsBaselineCompiledHelper(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::FunctionTemplate> t =
      v8::FunctionTemplate::New(isolate, IsBaselineCompiled);
  CHECK(context->Global()
            ->Set(context, v8_str("IsBaselineCompiled"),
                  t->GetFunction(context).ToLocalChecked())
            .FromJust());
}

TEST(IgnitionBaselineOnReturn) {
  // TODO(4280): Remove this entire test once --ignition-preserve-bytecode is
  // the default and the flag is removed. This test doesn't provide benefit any
  // longer once {InterpreterActivationsFinder} is gone.
  if (FLAG_ignition_preserve_bytecode) return;
  FLAG_allow_natives_syntax = true;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  FLAG_ignition = true;
  Isolate* isolate = CcTest::i_isolate();
  isolate->interpreter()->Initialize();
  v8::HandleScope scope(CcTest::isolate());
  InstallIsBaselineCompiledHelper(CcTest::isolate());

  CompileRun(
      "var is_baseline_in_function, is_baseline_after_return;\n"
      "var return_val;\n"
      "function f() {\n"
      "  %CompileBaseline(f);\n"
      "  is_baseline_in_function = IsBaselineCompiled(f);\n"
      "  return 1234;\n"
      "};\n"
      "return_val = f();\n"
      "is_baseline_after_return = IsBaselineCompiled(f);\n");
  CHECK_EQ(false, GetGlobalProperty("is_baseline_in_function")->BooleanValue());
  CHECK_EQ(true, GetGlobalProperty("is_baseline_after_return")->BooleanValue());
  CHECK_EQ(1234.0, GetGlobalProperty("return_val")->Number());
}

TEST(IgnitionEntryTrampolineSelfHealing) {
  FLAG_allow_natives_syntax = true;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  FLAG_ignition = true;
  Isolate* isolate = CcTest::i_isolate();
  isolate->interpreter()->Initialize();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "function MkFun() {"
      "  function f() { return 23 }"
      "  return f"
      "}"
      "var f1 = MkFun(); f1();"
      "var f2 = MkFun(); f2();"
      "%BaselineFunctionOnNextCall(f1);");
  Handle<JSFunction> f1 = Handle<JSFunction>::cast(GetGlobalProperty("f1"));
  Handle<JSFunction> f2 = Handle<JSFunction>::cast(GetGlobalProperty("f2"));

  // Function {f1} is marked for baseline.
  CompileRun("var result1 = f1()");
  CHECK_NE(*isolate->builtins()->InterpreterEntryTrampoline(), f1->code());
  CHECK_EQ(*isolate->builtins()->InterpreterEntryTrampoline(), f2->code());
  CHECK_EQ(23.0, GetGlobalProperty("result1")->Number());

  // Function {f2} will self-heal now.
  CompileRun("var result2 = f2()");
  CHECK_NE(*isolate->builtins()->InterpreterEntryTrampoline(), f1->code());
  CHECK_NE(*isolate->builtins()->InterpreterEntryTrampoline(), f2->code());
  CHECK_EQ(23.0, GetGlobalProperty("result2")->Number());
}

TEST(InvocationCount) {
  FLAG_allow_natives_syntax = true;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "function bar() {};"
      "function foo() { return bar(); };"
      "foo();");
  Handle<JSFunction> foo = Handle<JSFunction>::cast(GetGlobalProperty("foo"));
  CHECK_EQ(1, foo->feedback_vector()->invocation_count());
  CompileRun("foo()");
  CHECK_EQ(2, foo->feedback_vector()->invocation_count());
  CompileRun("bar()");
  CHECK_EQ(2, foo->feedback_vector()->invocation_count());
  CompileRun("foo(); foo()");
  CHECK_EQ(4, foo->feedback_vector()->invocation_count());
  CompileRun("%BaselineFunctionOnNextCall(foo);");
  CompileRun("foo();");
  CHECK_EQ(5, foo->feedback_vector()->invocation_count());
}
