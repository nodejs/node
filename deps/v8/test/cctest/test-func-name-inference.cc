// Copyright 2007-2009 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "api.h"
#include "runtime.h"
#include "cctest.h"


using ::v8::internal::CStrVector;
using ::v8::internal::Factory;
using ::v8::internal::Handle;
using ::v8::internal::Heap;
using ::v8::internal::JSFunction;
using ::v8::internal::Object;
using ::v8::internal::Runtime;
using ::v8::internal::Script;
using ::v8::internal::SmartPointer;
using ::v8::internal::SharedFunctionInfo;
using ::v8::internal::String;


static v8::Persistent<v8::Context> env;


static void InitializeVM() {
  if (env.IsEmpty()) {
    v8::HandleScope scope;
    env = v8::Context::New();
  }
  v8::HandleScope scope;
  env->Enter();
}


static void CheckFunctionName(v8::Handle<v8::Script> script,
                              const char* func_pos_src,
                              const char* ref_inferred_name) {
  // Get script source.
  Handle<JSFunction> fun = v8::Utils::OpenHandle(*script);
  Handle<Script> i_script(Script::cast(fun->shared()->script()));
  CHECK(i_script->source()->IsString());
  Handle<String> script_src(String::cast(i_script->source()));

  // Find the position of a given func source substring in the source.
  Handle<String> func_pos_str =
      Factory::NewStringFromAscii(CStrVector(func_pos_src));
  int func_pos = Runtime::StringMatch(script_src, func_pos_str, 0);
  CHECK_NE(0, func_pos);

  // Obtain SharedFunctionInfo for the function.
  Object* shared_func_info_ptr =
      Runtime::FindSharedFunctionInfoInScript(i_script, func_pos);
  CHECK(shared_func_info_ptr != Heap::undefined_value());
  Handle<SharedFunctionInfo> shared_func_info(
      SharedFunctionInfo::cast(shared_func_info_ptr));

  // Verify inferred function name.
  SmartPointer<char> inferred_name =
      shared_func_info->inferred_name()->ToCString();
  CHECK_EQ(ref_inferred_name, *inferred_name);
}


static v8::Handle<v8::Script> Compile(const char* src) {
  return v8::Script::Compile(v8::String::New(src));
}


TEST(GlobalProperty) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "fun1 = function() { return 1; }\n"
      "fun2 = function() { return 2; }\n");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun2");
}


TEST(GlobalVar) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "var fun1 = function() { return 1; }\n"
      "var fun2 = function() { return 2; }\n");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun2");
}


TEST(LocalVar) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "function outer() {\n"
      "  var fun1 = function() { return 1; }\n"
      "  var fun2 = function() { return 2; }\n"
      "}");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun2");
}


TEST(InConstructor) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "function MyClass() {\n"
      "  this.method1 = function() { return 1; }\n"
      "  this.method2 = function() { return 2; }\n"
      "}");
  CheckFunctionName(script, "return 1", "MyClass.method1");
  CheckFunctionName(script, "return 2", "MyClass.method2");
}


TEST(Factory) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "function createMyObj() {\n"
      "  var obj = {};\n"
      "  obj.method1 = function() { return 1; }\n"
      "  obj.method2 = function() { return 2; }\n"
      "  return obj;\n"
      "}");
  CheckFunctionName(script, "return 1", "obj.method1");
  CheckFunctionName(script, "return 2", "obj.method2");
}


TEST(Static) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "function MyClass() {}\n"
      "MyClass.static1 = function() { return 1; }\n"
      "MyClass.static2 = function() { return 2; }\n"
      "MyClass.MyInnerClass = {}\n"
      "MyClass.MyInnerClass.static3 = function() { return 3; }\n"
      "MyClass.MyInnerClass.static4 = function() { return 4; }");
  CheckFunctionName(script, "return 1", "MyClass.static1");
  CheckFunctionName(script, "return 2", "MyClass.static2");
  CheckFunctionName(script, "return 3", "MyClass.MyInnerClass.static3");
  CheckFunctionName(script, "return 4", "MyClass.MyInnerClass.static4");
}


TEST(Prototype) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "function MyClass() {}\n"
      "MyClass.prototype.method1 = function() { return 1; }\n"
      "MyClass.prototype.method2 = function() { return 2; }\n"
      "MyClass.MyInnerClass = function() {}\n"
      "MyClass.MyInnerClass.prototype.method3 = function() { return 3; }\n"
      "MyClass.MyInnerClass.prototype.method4 = function() { return 4; }");
  CheckFunctionName(script, "return 1", "MyClass.method1");
  CheckFunctionName(script, "return 2", "MyClass.method2");
  CheckFunctionName(script, "return 3", "MyClass.MyInnerClass.method3");
  CheckFunctionName(script, "return 4", "MyClass.MyInnerClass.method4");
}


TEST(ObjectLiteral) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "function MyClass() {}\n"
      "MyClass.prototype = {\n"
      "  method1: function() { return 1; },\n"
      "  method2: function() { return 2; } }");
  CheckFunctionName(script, "return 1", "MyClass.method1");
  CheckFunctionName(script, "return 2", "MyClass.method2");
}


TEST(AsParameter) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "function f1(a) { return a(); }\n"
      "function f2(a, b) { return a() + b(); }\n"
      "var result1 = f1(function() { return 1; })\n"
      "var result2 = f2(function() { return 2; }, function() { return 3; })");
  // Can't infer names here.
  CheckFunctionName(script, "return 1", "");
  CheckFunctionName(script, "return 2", "");
  CheckFunctionName(script, "return 3", "");
}


TEST(MultipleFuncsConditional) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "fun1 = 0 ?\n"
      "    function() { return 1; } :\n"
      "    function() { return 2; }");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun1");
}


TEST(MultipleFuncsInLiteral) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "function MyClass() {}\n"
      "MyClass.prototype = {\n"
      "  method1: 0 ? function() { return 1; } :\n"
      "               function() { return 2; } }");
  CheckFunctionName(script, "return 1", "MyClass.method1");
  CheckFunctionName(script, "return 2", "MyClass.method1");
}


// See http://code.google.com/p/v8/issues/detail?id=380
TEST(Issue380) {
  InitializeVM();
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = Compile(
      "function a() {\n"
      "var result = function(p,a,c,k,e,d)"
      "{return p}(\"if blah blah\",62,1976,\'a|b\'.split(\'|\'),0,{})\n"
      "}");
  CheckFunctionName(script, "return p", "");
}
