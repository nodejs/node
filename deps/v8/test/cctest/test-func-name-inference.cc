// Copyright 2011 the V8 project authors. All rights reserved.
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
#include "debug.h"
#include "runtime.h"
#include "cctest.h"


using ::v8::internal::CStrVector;
using ::v8::internal::Factory;
using ::v8::internal::Handle;
using ::v8::internal::Heap;
using ::v8::internal::Isolate;
using ::v8::internal::JSFunction;
using ::v8::internal::Object;
using ::v8::internal::Runtime;
using ::v8::internal::Script;
using ::v8::internal::SmartArrayPointer;
using ::v8::internal::SharedFunctionInfo;
using ::v8::internal::String;


static void CheckFunctionName(v8::Handle<v8::Script> script,
                              const char* func_pos_src,
                              const char* ref_inferred_name) {
  // Get script source.
  Handle<Object> obj = v8::Utils::OpenHandle(*script);
  Handle<SharedFunctionInfo> shared_function;
  if (obj->IsSharedFunctionInfo()) {
    shared_function =
        Handle<SharedFunctionInfo>(SharedFunctionInfo::cast(*obj));
  } else {
    shared_function =
        Handle<SharedFunctionInfo>(JSFunction::cast(*obj)->shared());
  }
  Handle<Script> i_script(Script::cast(shared_function->script()));
  CHECK(i_script->source()->IsString());
  Handle<String> script_src(String::cast(i_script->source()));

  // Find the position of a given func source substring in the source.
  Handle<String> func_pos_str =
      FACTORY->NewStringFromAscii(CStrVector(func_pos_src));
  int func_pos = Runtime::StringMatch(Isolate::Current(),
                                      script_src,
                                      func_pos_str,
                                      0);
  CHECK_NE(0, func_pos);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Obtain SharedFunctionInfo for the function.
  Isolate::Current()->debug()->PrepareForBreakPoints();
  Object* shared_func_info_ptr =
      Isolate::Current()->debug()->FindSharedFunctionInfoInScript(i_script,
                                                                  func_pos);
  CHECK(shared_func_info_ptr != HEAP->undefined_value());
  Handle<SharedFunctionInfo> shared_func_info(
      SharedFunctionInfo::cast(shared_func_info_ptr));

  // Verify inferred function name.
  SmartArrayPointer<char> inferred_name =
      shared_func_info->inferred_name()->ToCString();
  CHECK_EQ(ref_inferred_name, *inferred_name);
#endif  // ENABLE_DEBUGGER_SUPPORT
}


static v8::Handle<v8::Script> Compile(const char* src) {
  return v8::Script::Compile(v8::String::New(src));
}


TEST(GlobalProperty) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "fun1 = function() { return 1; }\n"
      "fun2 = function() { return 2; }\n");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun2");
}


TEST(GlobalVar) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "var fun1 = function() { return 1; }\n"
      "var fun2 = function() { return 2; }\n");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun2");
}


TEST(LocalVar) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "function outer() {\n"
      "  var fun1 = function() { return 1; }\n"
      "  var fun2 = function() { return 2; }\n"
      "}");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun2");
}


TEST(InConstructor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "function MyClass() {\n"
      "  this.method1 = function() { return 1; }\n"
      "  this.method2 = function() { return 2; }\n"
      "}");
  CheckFunctionName(script, "return 1", "MyClass.method1");
  CheckFunctionName(script, "return 2", "MyClass.method2");
}


TEST(Factory) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

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
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

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
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

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
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "function MyClass() {}\n"
      "MyClass.prototype = {\n"
      "  method1: function() { return 1; },\n"
      "  method2: function() { return 2; } }");
  CheckFunctionName(script, "return 1", "MyClass.method1");
  CheckFunctionName(script, "return 2", "MyClass.method2");
}


TEST(AsParameter) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

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
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "fun1 = 0 ?\n"
      "    function() { return 1; } :\n"
      "    function() { return 2; }");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun1");
}


TEST(MultipleFuncsInLiteral) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

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
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "function a() {\n"
      "var result = function(p,a,c,k,e,d)"
      "{return p}(\"if blah blah\",62,1976,\'a|b\'.split(\'|\'),0,{})\n"
      "}");
  CheckFunctionName(script, "return p", "");
}


TEST(MultipleAssignments) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "var fun1 = fun2 = function () { return 1; }\n"
      "var bar1 = bar2 = bar3 = function () { return 2; }\n"
      "foo1 = foo2 = function () { return 3; }\n"
      "baz1 = baz2 = baz3 = function () { return 4; }");
  CheckFunctionName(script, "return 1", "fun2");
  CheckFunctionName(script, "return 2", "bar3");
  CheckFunctionName(script, "return 3", "foo2");
  CheckFunctionName(script, "return 4", "baz3");
}


TEST(AsConstructorParameter) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "function Foo() {}\n"
      "var foo = new Foo(function() { return 1; })\n"
      "var bar = new Foo(function() { return 2; }, function() { return 3; })");
  CheckFunctionName(script, "return 1", "");
  CheckFunctionName(script, "return 2", "");
  CheckFunctionName(script, "return 3", "");
}


TEST(FactoryHashmap) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "function createMyObj() {\n"
      "  var obj = {};\n"
      "  obj[\"method1\"] = function() { return 1; }\n"
      "  obj[\"method2\"] = function() { return 2; }\n"
      "  return obj;\n"
      "}");
  CheckFunctionName(script, "return 1", "obj.method1");
  CheckFunctionName(script, "return 2", "obj.method2");
}


TEST(FactoryHashmapVariable) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "function createMyObj() {\n"
      "  var obj = {};\n"
      "  var methodName = \"method1\";\n"
      "  obj[methodName] = function() { return 1; }\n"
      "  methodName = \"method2\";\n"
      "  obj[methodName] = function() { return 2; }\n"
      "  return obj;\n"
      "}");
  // Can't infer function names statically.
  CheckFunctionName(script, "return 1", "obj.(anonymous function)");
  CheckFunctionName(script, "return 2", "obj.(anonymous function)");
}


TEST(FactoryHashmapConditional) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "function createMyObj() {\n"
      "  var obj = {};\n"
      "  obj[0 ? \"method1\" : \"method2\"] = function() { return 1; }\n"
      "  return obj;\n"
      "}");
  // Can't infer the function name statically.
  CheckFunctionName(script, "return 1", "obj.(anonymous function)");
}


TEST(GlobalAssignmentAndCall) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "var Foo = function() {\n"
      "  return 1;\n"
      "}();\n"
      "var Baz = Bar = function() {\n"
      "  return 2;\n"
      "}");
  // The inferred name is empty, because this is an assignment of a result.
  CheckFunctionName(script, "return 1", "");
  // See MultipleAssignments test.
  CheckFunctionName(script, "return 2", "Bar");
}


TEST(AssignmentAndCall) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "(function Enclosing() {\n"
      "  var Foo;\n"
      "  Foo = function() {\n"
      "    return 1;\n"
      "  }();\n"
      "  var Baz = Bar = function() {\n"
      "    return 2;\n"
      "  }\n"
      "})();");
  // The inferred name is empty, because this is an assignment of a result.
  CheckFunctionName(script, "return 1", "");
  // See MultipleAssignments test.
  // TODO(2276): Lazy compiling the enclosing outer closure would yield
  // in "Enclosing.Bar" being the inferred name here.
  CheckFunctionName(script, "return 2", "Bar");
}


TEST(MethodAssignmentInAnonymousFunctionCall) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "(function () {\n"
      "    var EventSource = function () { };\n"
      "    EventSource.prototype.addListener = function () {\n"
      "        return 2012;\n"
      "    };\n"
      "    this.PublicEventSource = EventSource;\n"
      "})();");
  CheckFunctionName(script, "return 2012", "EventSource.addListener");
}


TEST(ReturnAnonymousFunction) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::Script> script = Compile(
      "(function() {\n"
      "  function wrapCode() {\n"
      "    return function () {\n"
      "      return 2012;\n"
      "    };\n"
      "  };\n"
      "  var foo = 10;\n"
      "  function f() {\n"
      "    return wrapCode();\n"
      "  }\n"
      "  this.ref = f;\n"
      "})()");
  script->Run();
  CheckFunctionName(script, "return 2012", "");
}
