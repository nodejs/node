// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
#include <wchar.h>  // wint_t

#include "v8.h"

#include "compiler.h"
#include "execution.h"
#include "factory.h"
#include "platform.h"
#include "top.h"
#include "cctest.h"

using namespace v8::internal;

static v8::Persistent<v8::Context> env;

// --- P r i n t   E x t e n s i o n ---

class PrintExtension : public v8::Extension {
 public:
  PrintExtension() : v8::Extension("v8/print", kSource) { }
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name);
  static v8::Handle<v8::Value> Print(const v8::Arguments& args);
 private:
  static const char* kSource;
};


const char* PrintExtension::kSource = "native function print();";


v8::Handle<v8::FunctionTemplate> PrintExtension::GetNativeFunction(
    v8::Handle<v8::String> str) {
  return v8::FunctionTemplate::New(PrintExtension::Print);
}


v8::Handle<v8::Value> PrintExtension::Print(const v8::Arguments& args) {
  for (int i = 0; i < args.Length(); i++) {
    if (i != 0) printf(" ");
    v8::HandleScope scope;
    v8::Handle<v8::Value> arg = args[i];
    v8::Handle<v8::String> string_obj = arg->ToString();
    if (string_obj.IsEmpty()) return string_obj;
    int length = string_obj->Length();
    uint16_t* string = NewArray<uint16_t>(length + 1);
    string_obj->Write(string);
    for (int j = 0; j < length; j++)
      printf("%lc", static_cast<wint_t>(string[j]));
    DeleteArray(string);
  }
  printf("\n");
  return v8::Undefined();
}


static PrintExtension kPrintExtension;
v8::DeclareExtension kPrintExtensionDeclaration(&kPrintExtension);


static void InitializeVM() {
  if (env.IsEmpty()) {
    v8::HandleScope scope;
    const char* extensions[] = { "v8/print", "v8/gc" };
    v8::ExtensionConfiguration config(2, extensions);
    env = v8::Context::New(&config);
  }
  v8::HandleScope scope;
  env->Enter();
}


static Object* GetGlobalProperty(const char* name) {
  Handle<String> symbol = Factory::LookupAsciiSymbol(name);
  return Top::context()->global()->GetProperty(*symbol);
}


static void SetGlobalProperty(const char* name, Object* value) {
  Handle<Object> object(value);
  Handle<String> symbol = Factory::LookupAsciiSymbol(name);
  Handle<JSObject> global(Top::context()->global());
  SetProperty(global, symbol, object, NONE);
}


static Handle<JSFunction> Compile(const char* source) {
  Handle<String> source_code(Factory::NewStringFromUtf8(CStrVector(source)));
  Handle<JSFunction> boilerplate =
      Compiler::Compile(source_code,
                        Handle<String>(),
                        0,
                        0,
                        NULL,
                        NULL,
                        Handle<String>::null(),
                        NOT_NATIVES_CODE);
  return Factory::NewFunctionFromBoilerplate(boilerplate,
                                             Top::global_context());
}


static double Inc(int x) {
  const char* source = "result = %d + 1;";
  EmbeddedVector<char, 512> buffer;
  OS::SNPrintF(buffer, source, x);

  Handle<JSFunction> fun = Compile(buffer.start());
  if (fun.is_null()) return -1;

  bool has_pending_exception;
  Handle<JSObject> global(Top::context()->global());
  Execution::Call(fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
  return GetGlobalProperty("result")->Number();
}


TEST(Inc) {
  InitializeVM();
  v8::HandleScope scope;
  CHECK_EQ(4.0, Inc(3));
}


static double Add(int x, int y) {
  Handle<JSFunction> fun = Compile("result = x + y;");
  if (fun.is_null()) return -1;

  SetGlobalProperty("x", Smi::FromInt(x));
  SetGlobalProperty("y", Smi::FromInt(y));
  bool has_pending_exception;
  Handle<JSObject> global(Top::context()->global());
  Execution::Call(fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
  return GetGlobalProperty("result")->Number();
}


TEST(Add) {
  InitializeVM();
  v8::HandleScope scope;
  CHECK_EQ(5.0, Add(2, 3));
}


static double Abs(int x) {
  Handle<JSFunction> fun = Compile("if (x < 0) result = -x; else result = x;");
  if (fun.is_null()) return -1;

  SetGlobalProperty("x", Smi::FromInt(x));
  bool has_pending_exception;
  Handle<JSObject> global(Top::context()->global());
  Execution::Call(fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
  return GetGlobalProperty("result")->Number();
}


TEST(Abs) {
  InitializeVM();
  v8::HandleScope scope;
  CHECK_EQ(3.0, Abs(-3));
}


static double Sum(int n) {
  Handle<JSFunction> fun =
      Compile("s = 0; while (n > 0) { s += n; n -= 1; }; result = s;");
  if (fun.is_null()) return -1;

  SetGlobalProperty("n", Smi::FromInt(n));
  bool has_pending_exception;
  Handle<JSObject> global(Top::context()->global());
  Execution::Call(fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
  return GetGlobalProperty("result")->Number();
}


TEST(Sum) {
  InitializeVM();
  v8::HandleScope scope;
  CHECK_EQ(5050.0, Sum(100));
}


TEST(Print) {
  InitializeVM();
  v8::HandleScope scope;
  const char* source = "for (n = 0; n < 100; ++n) print(n, 1, 2);";
  Handle<JSFunction> fun = Compile(source);
  if (fun.is_null()) return;
  bool has_pending_exception;
  Handle<JSObject> global(Top::context()->global());
  Execution::Call(fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
}


// The following test method stems from my coding efforts today. It
// tests all the functionality I have added to the compiler today
TEST(Stuff) {
  InitializeVM();
  v8::HandleScope scope;
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
  Handle<JSObject> global(Top::context()->global());
  Execution::Call(fun, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);
  CHECK_EQ(511.0, GetGlobalProperty("r")->Number());
}


TEST(UncaughtThrow) {
  InitializeVM();
  v8::HandleScope scope;

  const char* source = "throw 42;";
  Handle<JSFunction> fun = Compile(source);
  CHECK(!fun.is_null());
  bool has_pending_exception;
  Handle<JSObject> global(Top::context()->global());
  Handle<Object> result =
      Execution::Call(fun, global, 0, NULL, &has_pending_exception);
  CHECK(has_pending_exception);
  CHECK_EQ(42.0, Top::pending_exception()->Number());
}


// Tests calling a builtin function from C/C++ code, and the builtin function
// performs GC. It creates a stack frame looks like following:
//   | C (PerformGC) |
//   |   JS-to-C     |
//   |      JS       |
//   |   C-to-JS     |
TEST(C2JSFrames) {
  InitializeVM();
  v8::HandleScope scope;

  const char* source = "function foo(a) { gc(), print(a); }";

  Handle<JSFunction> fun0 = Compile(source);
  CHECK(!fun0.is_null());

  // Run the generated code to populate the global object with 'foo'.
  bool has_pending_exception;
  Handle<JSObject> global(Top::context()->global());
  Execution::Call(fun0, global, 0, NULL, &has_pending_exception);
  CHECK(!has_pending_exception);

  Handle<Object> fun1 =
      Handle<Object>(
          Top::context()->global()->GetProperty(
              *Factory::LookupAsciiSymbol("foo")));
  CHECK(fun1->IsJSFunction());

  Object** argv[1] = {
    Handle<Object>::cast(Factory::LookupAsciiSymbol("hello")).location()
  };
  Execution::Call(Handle<JSFunction>::cast(fun1), global, 1, argv,
                  &has_pending_exception);
  CHECK(!has_pending_exception);
}


// Regression 236. Calling InitLineEnds on a Script with undefined
// source resulted in crash.
TEST(Regression236) {
  InitializeVM();
  v8::HandleScope scope;

  Handle<Script> script = Factory::NewScript(Factory::empty_string());
  script->set_source(Heap::undefined_value());
  CHECK_EQ(-1, GetScriptLineNumber(script, 0));
  CHECK_EQ(-1, GetScriptLineNumber(script, 100));
  CHECK_EQ(-1, GetScriptLineNumber(script, -1));
}


TEST(GetScriptLineNumber) {
  LocalContext env;
  v8::HandleScope scope;
  v8::ScriptOrigin origin = v8::ScriptOrigin(v8::String::New("test"));
  const char function_f[] = "function f() {}";
  const int max_rows = 1000;
  const int buffer_size = max_rows + sizeof(function_f);
  ScopedVector<char> buffer(buffer_size);
  memset(buffer.start(), '\n', buffer_size - 1);
  buffer[buffer_size - 1] = '\0';

  for (int i = 0; i < max_rows; ++i) {
    if (i > 0)
      buffer[i - 1] = '\n';
    memcpy(&buffer[i], function_f, sizeof(function_f) - 1);
    v8::Handle<v8::String> script_body = v8::String::New(buffer.start());
    v8::Script::Compile(script_body, &origin)->Run();
    v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
        env->Global()->Get(v8::String::New("f")));
    CHECK_EQ(i, f->GetScriptLineNumber());
  }
}
