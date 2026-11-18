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

#include <memory>

#include "src/api/api-inl.h"
#include "src/debug/debug.h"
#include "src/objects/objects-inl.h"
#include "src/strings/string-search.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace func_name_inference_unittest {

class FuncNameInferenceTest : public TestWithNativeContext {
 protected:
  void CheckFunctionName(v8::Local<v8::Script> script, const char* func_pos_src,
                         const char* ref_inferred_name) {
    Isolate* isolate = i_isolate();

    // Get script source.
    DirectHandle<Object> obj = v8::Utils::OpenDirectHandle(*script);
    DirectHandle<SharedFunctionInfo> shared_function(
        IsSharedFunctionInfo(*obj) ? Cast<SharedFunctionInfo>(*obj)
                                   : Cast<JSFunction>(*obj)->shared(),
        isolate);
    Handle<Script> i_script(Cast<Script>(shared_function->script()), isolate);
    CHECK(IsString(i_script->source()));
    DirectHandle<String> script_src(Cast<String>(i_script->source()), isolate);

    // Find the position of a given func source substring in the source.
    int func_pos;
    {
      DisallowGarbageCollection no_gc;
      v8::base::Vector<const uint8_t> func_pos_str =
          v8::base::OneByteVector(func_pos_src);
      String::FlatContent script_content = script_src->GetFlatContent(no_gc);
      func_pos = SearchString(isolate, script_content.ToOneByteVector(),
                              func_pos_str, 0);
    }
    CHECK_NE(0, func_pos);

    // Obtain SharedFunctionInfo for the function.
    DirectHandle<SharedFunctionInfo> shared_func_info =
        Cast<SharedFunctionInfo>(
            isolate->debug()->FindInnermostContainingFunctionInfo(i_script,
                                                                  func_pos));

    // Verify inferred function name.
    std::unique_ptr<char[]> inferred_name =
        shared_func_info->inferred_name()->ToCString();
    EXPECT_STREQ(ref_inferred_name, inferred_name.get());
  }

  v8::Local<v8::Script> Compile(v8::Isolate* isolate, const char* src) {
    return v8::Script::Compile(
               v8_context(),
               v8::String::NewFromUtf8(isolate, src).ToLocalChecked())
        .ToLocalChecked();
  }
};

TEST_F(FuncNameInferenceTest, GlobalProperty) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
                                         "fun1 = function() { return 1; }\n"
                                         "fun2 = function() { return 2; }\n");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun2");
}

TEST_F(FuncNameInferenceTest, GlobalVar) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "var fun1 = function() { return 1; }\n"
              "var fun2 = function() { return 2; }\n");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun2");
}

TEST_F(FuncNameInferenceTest, LocalVar) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "function outer() {\n"
              "  var fun1 = function() { return 1; }\n"
              "  var fun2 = function() { return 2; }\n"
              "}");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun2");
}

TEST_F(FuncNameInferenceTest, ObjectProperty) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "var obj = {\n"
              "  fun1: function() { return 1; },\n"
              "  fun2: class { constructor() { return 2; } }\n"
              "}");
  CheckFunctionName(script, "return 1", "obj.fun1");
  CheckFunctionName(script, "return 2", "obj.fun2");
}

TEST_F(FuncNameInferenceTest, InConstructor) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "function MyClass() {\n"
              "  this.method1 = function() { return 1; }\n"
              "  this.method2 = function() { return 2; }\n"
              "}");
  CheckFunctionName(script, "return 1", "MyClass.method1");
  CheckFunctionName(script, "return 2", "MyClass.method2");
}

TEST_F(FuncNameInferenceTest, Factory) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "function createMyObj() {\n"
              "  var obj = {};\n"
              "  obj.method1 = function() { return 1; }\n"
              "  obj.method2 = function() { return 2; }\n"
              "  return obj;\n"
              "}");
  CheckFunctionName(script, "return 1", "obj.method1");
  CheckFunctionName(script, "return 2", "obj.method2");
}

TEST_F(FuncNameInferenceTest, Static) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
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

TEST_F(FuncNameInferenceTest, Prototype) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(
      v8_isolate(),
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

TEST_F(FuncNameInferenceTest, ObjectLiteral) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "function MyClass() {}\n"
              "MyClass.prototype = {\n"
              "  method1: function() { return 1; },\n"
              "  method2: function() { return 2; } }");
  CheckFunctionName(script, "return 1", "MyClass.method1");
  CheckFunctionName(script, "return 2", "MyClass.method2");
}

TEST_F(FuncNameInferenceTest, UpperCaseClass) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
                                         "'use strict';\n"
                                         "class MyClass {\n"
                                         "  constructor() {\n"
                                         "    this.value = 1;\n"
                                         "  }\n"
                                         "  method() {\n"
                                         "    this.value = 2;\n"
                                         "  }\n"
                                         "}");
  CheckFunctionName(script, "this.value = 1", "MyClass");
  CheckFunctionName(script, "this.value = 2", "MyClass.method");
}

TEST_F(FuncNameInferenceTest, LowerCaseClass) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
                                         "'use strict';\n"
                                         "class myclass {\n"
                                         "  constructor() {\n"
                                         "    this.value = 1;\n"
                                         "  }\n"
                                         "  method() {\n"
                                         "    this.value = 2;\n"
                                         "  }\n"
                                         "}");
  CheckFunctionName(script, "this.value = 1", "myclass");
  CheckFunctionName(script, "this.value = 2", "myclass.method");
}

TEST_F(FuncNameInferenceTest, AsParameter) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(
      v8_isolate(),
      "function f1(a) { return a(); }\n"
      "function f2(a, b) { return a() + b(); }\n"
      "var result1 = f1(function() { return 1; })\n"
      "var result2 = f2(function() { return 2; }, function() { return 3; })");
  // Can't infer names here.
  CheckFunctionName(script, "return 1", "");
  CheckFunctionName(script, "return 2", "");
  CheckFunctionName(script, "return 3", "");
}

TEST_F(FuncNameInferenceTest, MultipleFuncsConditional) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
                                         "var x = 0;\n"
                                         "fun1 = x ?\n"
                                         "    function() { return 1; } :\n"
                                         "    function() { return 2; }");
  CheckFunctionName(script, "return 1", "fun1");
  CheckFunctionName(script, "return 2", "fun1");
}

TEST_F(FuncNameInferenceTest, MultipleFuncsInLiteral) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "var x = 0;\n"
              "function MyClass() {}\n"
              "MyClass.prototype = {\n"
              "  method1: x ? function() { return 1; } :\n"
              "               function() { return 2; } }");
  CheckFunctionName(script, "return 1", "MyClass.method1");
  CheckFunctionName(script, "return 2", "MyClass.method1");
}

TEST_F(FuncNameInferenceTest, AnonymousInAnonymousClosure1) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
                                         "(function() {\n"
                                         "  (function() {\n"
                                         "      var a = 1;\n"
                                         "      return;\n"
                                         "  })();\n"
                                         "  var b = function() {\n"
                                         "      var c = 1;\n"
                                         "      return;\n"
                                         "  };\n"
                                         "})();");
  CheckFunctionName(script, "return", "");
}

TEST_F(FuncNameInferenceTest, AnonymousInAnonymousClosure2) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
                                         "(function() {\n"
                                         "  (function() {\n"
                                         "      var a = 1;\n"
                                         "      return;\n"
                                         "  })();\n"
                                         "  var c = 1;\n"
                                         "})();");
  CheckFunctionName(script, "return", "");
}

TEST_F(FuncNameInferenceTest, NamedInAnonymousClosure) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
                                         "var foo = function() {\n"
                                         "  (function named() {\n"
                                         "      var a = 1;\n"
                                         "  })();\n"
                                         "  var c = 1;\n"
                                         "  return;\n"
                                         "};");
  CheckFunctionName(script, "return", "foo");
}

// See http://code.google.com/p/v8/issues/detail?id=380
TEST_F(FuncNameInferenceTest, Issue380) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "function a() {\n"
              "var result = function(p,a,c,k,e,d)"
              "{return p}(\"if blah blah\",62,1976,\'a|b\'.split(\'|\'),0,{})\n"
              "}");
  CheckFunctionName(script, "return p", "");
}

TEST_F(FuncNameInferenceTest, MultipleAssignments) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "var fun1 = fun2 = function () { return 1; }\n"
              "var bar1 = bar2 = bar3 = function () { return 2; }\n"
              "foo1 = foo2 = function () { return 3; }\n"
              "baz1 = baz2 = baz3 = function () { return 4; }");
  CheckFunctionName(script, "return 1", "fun2");
  CheckFunctionName(script, "return 2", "bar3");
  CheckFunctionName(script, "return 3", "foo2");
  CheckFunctionName(script, "return 4", "baz3");
}

TEST_F(FuncNameInferenceTest, AsConstructorParameter) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(
      v8_isolate(),
      "function Foo() {}\n"
      "var foo = new Foo(function() { return 1; })\n"
      "var bar = new Foo(function() { return 2; }, function() { return 3; })");
  CheckFunctionName(script, "return 1", "");
  CheckFunctionName(script, "return 2", "");
  CheckFunctionName(script, "return 3", "");
}

TEST_F(FuncNameInferenceTest, FactoryHashmap) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "function createMyObj() {\n"
              "  var obj = {};\n"
              "  obj[\"method1\"] = function() { return 1; }\n"
              "  obj[\"method2\"] = function() { return 2; }\n"
              "  return obj;\n"
              "}");
  CheckFunctionName(script, "return 1", "obj.method1");
  CheckFunctionName(script, "return 2", "obj.method2");
}

TEST_F(FuncNameInferenceTest, FactoryHashmapVariable) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "function createMyObj() {\n"
              "  var obj = {};\n"
              "  var methodName = \"method1\";\n"
              "  obj[methodName] = function() { return 1; }\n"
              "  methodName = \"method2\";\n"
              "  obj[methodName] = function() { return 2; }\n"
              "  return obj;\n"
              "}");
  // Can't infer function names statically.
  CheckFunctionName(script, "return 1", "obj.<computed>");
  CheckFunctionName(script, "return 2", "obj.<computed>");
}

TEST_F(FuncNameInferenceTest, FactoryHashmapConditional) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(
      v8_isolate(),
      "function createMyObj() {\n"
      "  var obj = {};\n"
      "  obj[0 ? \"method1\" : \"method2\"] = function() { return 1; }\n"
      "  return obj;\n"
      "}");
  // Can't infer the function name statically.
  CheckFunctionName(script, "return 1", "obj.<computed>");
}

TEST_F(FuncNameInferenceTest, GlobalAssignmentAndCall) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
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

TEST_F(FuncNameInferenceTest, AssignmentAndCall) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
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

TEST_F(FuncNameInferenceTest, MethodAssignmentInAnonymousFunctionCall) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "(function () {\n"
              "    var EventSource = function () { };\n"
              "    EventSource.prototype.addListener = function () {\n"
              "        return 2012;\n"
              "    };\n"
              "    this.PublicEventSource = EventSource;\n"
              "})();");
  CheckFunctionName(script, "return 2012", "EventSource.addListener");
}

TEST_F(FuncNameInferenceTest, ReturnAnonymousFunction) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
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
  script->Run(v8_context()).ToLocalChecked();
  CheckFunctionName(script, "return 2012", "");
}

TEST_F(FuncNameInferenceTest, IgnoreExtendsClause) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script =
      Compile(v8_isolate(),
              "(function() {\n"
              "  var foo = {};\n"
              "  foo.C = class {}\n"
              "  class D extends foo.C {}\n"
              "  foo.bar = function() { return 1; };\n"
              "})()");
  script->Run(v8_context()).ToLocalChecked();
  CheckFunctionName(script, "return 1", "foo.bar");
}

TEST_F(FuncNameInferenceTest, ParameterAndArrow) {
  v8::HandleScope scope(v8_isolate());

  v8::Local<v8::Script> script = Compile(v8_isolate(),
                                         "(function(param) {\n"
                                         "  (() => { return 2017 })();\n"
                                         "})()");
  script->Run(v8_context()).ToLocalChecked();
  CheckFunctionName(script, "return 2017", "");
}

}  // namespace func_name_inference_unittest
}  // namespace internal
}  // namespace v8
