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

#include "v8.h"

#include "compiler.h"
#include "disasm.h"
#include "cctest.h"

using namespace v8::internal;

static MaybeObject* GetGlobalProperty(const char* name) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<String> internalized_name =
      isolate->factory()->InternalizeUtf8String(name);
  return isolate->context()->global_object()->GetProperty(*internalized_name);
}


static void SetGlobalProperty(const char* name, Object* value) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<Object> object(value, isolate);
  Handle<String> internalized_name =
      isolate->factory()->InternalizeUtf8String(name);
  Handle<JSObject> global(isolate->context()->global_object());
  Runtime::SetObjectProperty(isolate, global, internalized_name, object, NONE,
                             kNonStrictMode);
}


static Handle<JSFunction> Compile(const char* source) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<String> source_code(
      isolate->factory()->NewStringFromUtf8(CStrVector(source)));
  Handle<SharedFunctionInfo> shared_function =
      Compiler::CompileScript(source_code,
                              Handle<String>(),
                              0,
                              0,
                              false,
                              Handle<Context>(isolate->native_context()),
                              NULL, NULL,
                              Handle<String>::null(),
                              NOT_NATIVES_CODE);
  return isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared_function, isolate->native_context());
}


static double Inc(Isolate* isolate, int x) {
  const char* source = "result = %d + 1;";
  EmbeddedVector<char, 512> buffer;
  OS::SNPrintF(buffer, source, x);

  Handle<JSFunction> fun = Compile(buffer.start());
  if (fun.is_null()) return -1;

  bool has_pending_exception;
  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(isolate, fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
  return GetGlobalProperty("result")->ToObjectChecked()->Number();
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
  bool has_pending_exception;
  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(isolate, fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
  return GetGlobalProperty("result")->ToObjectChecked()->Number();
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
  bool has_pending_exception;
  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(isolate, fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
  return GetGlobalProperty("result")->ToObjectChecked()->Number();
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
  bool has_pending_exception;
  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(isolate, fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
  return GetGlobalProperty("result")->ToObjectChecked()->Number();
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
  bool has_pending_exception;
  Handle<JSObject> global(CcTest::i_isolate()->context()->global_object());
  Execution::Call(
      CcTest::i_isolate(), fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
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
  bool has_pending_exception;
  Handle<JSObject> global(CcTest::i_isolate()->context()->global_object());
  Execution::Call(
      CcTest::i_isolate(), fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
  CHECK_EQ(511.0, GetGlobalProperty("r")->ToObjectChecked()->Number());
}


TEST(UncaughtThrow) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  const char* source = "throw 42;";
  Handle<JSFunction> fun = Compile(source);
  CHECK(!fun.is_null());
  bool has_pending_exception;
  Isolate* isolate = fun->GetIsolate();
  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(isolate, fun, global, 0, NULL, &has_pending_exception);
  CHECK(has_pending_exception);
  CHECK_EQ(42.0, isolate->pending_exception()->ToObjectChecked()->Number());
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
  bool has_pending_exception;
  Handle<JSObject> global(isolate->context()->global_object());
  Execution::Call(
      isolate, fun0, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);

  Object* foo_string = isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("foo"))->ToObjectChecked();
  MaybeObject* fun1_object = isolate->context()->global_object()->
      GetProperty(String::cast(foo_string));
  Handle<Object> fun1(fun1_object->ToObjectChecked(), isolate);
  CHECK(fun1->IsJSFunction());

  Handle<Object> argv[] = { isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("hello")) };
  Execution::Call(isolate,
                  Handle<JSFunction>::cast(fun1),
                  global,
                  ARRAY_SIZE(argv),
                  argv,
                  &has_pending_exception);
  CHECK(!has_pending_exception);
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
  CHECK_EQ(-1, GetScriptLineNumber(script, 0));
  CHECK_EQ(-1, GetScriptLineNumber(script, 100));
  CHECK_EQ(-1, GetScriptLineNumber(script, -1));
}


TEST(GetScriptLineNumber) {
  LocalContext context;
  v8::HandleScope scope(CcTest::isolate());
  v8::ScriptOrigin origin =
      v8::ScriptOrigin(v8::String::NewFromUtf8(CcTest::isolate(), "test"));
  const char function_f[] = "function f() {}";
  const int max_rows = 1000;
  const int buffer_size = max_rows + sizeof(function_f);
  ScopedVector<char> buffer(buffer_size);
  memset(buffer.start(), '\n', buffer_size - 1);
  buffer[buffer_size - 1] = '\0';

  for (int i = 0; i < max_rows; ++i) {
    if (i > 0)
      buffer[i - 1] = '\n';
    OS::MemCopy(&buffer[i], function_f, sizeof(function_f) - 1);
    v8::Handle<v8::String> script_body =
        v8::String::NewFromUtf8(CcTest::isolate(), buffer.start());
    v8::Script::Compile(script_body, &origin)->Run();
    v8::Local<v8::Function> f =
        v8::Local<v8::Function>::Cast(context->Global()->Get(
            v8::String::NewFromUtf8(CcTest::isolate(), "f")));
    CHECK_EQ(i, f->GetScriptLineNumber());
  }
}


// Test that optimized code for different closures is actually shared
// immediately by the FastNewClosureStub when run in the same context.
TEST(OptimizedCodeSharing) {
  // Skip test if --cache-optimized-code is not activated by default because
  // FastNewClosureStub that is baked into the snapshot is incorrect.
  if (!FLAG_cache_optimized_code) return;
  FLAG_stress_compaction = false;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  for (int i = 0; i < 10; i++) {
    LocalContext env;
    env->Global()->Set(v8::String::NewFromUtf8(CcTest::isolate(), "x"),
                       v8::Integer::New(CcTest::isolate(), i));
    CompileRun("function MakeClosure() {"
               "  return function() { return x; };"
               "}"
               "var closure0 = MakeClosure();"
               "%DebugPrint(closure0());"
               "%OptimizeFunctionOnNextCall(closure0);"
               "%DebugPrint(closure0());"
               "var closure1 = MakeClosure();"
               "var closure2 = MakeClosure();");
    Handle<JSFunction> fun1 = v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(env->Global()->Get(v8_str("closure1"))));
    Handle<JSFunction> fun2 = v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(env->Global()->Get(v8_str("closure2"))));
    CHECK(fun1->IsOptimized()
          || !CcTest::i_isolate()->use_crankshaft() || !fun1->IsOptimizable());
    CHECK(fun2->IsOptimized()
          || !CcTest::i_isolate()->use_crankshaft() || !fun2->IsOptimizable());
    CHECK_EQ(fun1->code(), fun2->code());
  }
}


#ifdef ENABLE_DISASSEMBLER
static Handle<JSFunction> GetJSFunction(v8::Handle<v8::Object> obj,
                                 const char* property_name) {
  v8::Local<v8::Function> fun =
      v8::Local<v8::Function>::Cast(obj->Get(v8_str(property_name)));
  return v8::Utils::OpenHandle(*fun);
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
    Address end = pc + decode_size;

    v8::internal::EmbeddedVector<char, 128> decode_buffer;
    v8::internal::EmbeddedVector<char, 128> smi_hex_buffer;
    Smi* smi = Smi::FromInt(12345678);
    OS::SNPrintF(smi_hex_buffer, "0x%lx", reinterpret_cast<intptr_t>(smi));
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
