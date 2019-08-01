// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags/flags.h"
#include "src/objects/contexts.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

TEST(SimpleCall) {
  FunctionTester T("(function(foo,a) { return foo(a); })");
  Handle<JSFunction> foo = T.NewFunction("(function(a) { return a; })");

  T.CheckCall(T.Val(3), foo, T.Val(3));
  T.CheckCall(T.Val(3.1), foo, T.Val(3.1));
  T.CheckCall(foo, foo, foo);
  T.CheckCall(T.Val("Abba"), foo, T.Val("Abba"));
}


TEST(SimpleCall2) {
  FunctionTester T("(function(foo,a) { return foo(a); })");
  FunctionTester U("(function(a) { return a; })");

  T.CheckCall(T.Val(3), U.function, T.Val(3));
  T.CheckCall(T.Val(3.1), U.function, T.Val(3.1));
  T.CheckCall(U.function, U.function, U.function);
  T.CheckCall(T.Val("Abba"), U.function, T.Val("Abba"));
}


TEST(ConstCall) {
  FunctionTester T("(function(foo,a) { return foo(a,3); })");
  FunctionTester U("(function (a,b) { return a + b; })");

  T.CheckCall(T.Val(6), U.function, T.Val(3));
  T.CheckCall(T.Val(6.1), U.function, T.Val(3.1));
  T.CheckCall(T.Val("function (a,b) { return a + b; }3"), U.function,
              U.function);
  T.CheckCall(T.Val("Abba3"), U.function, T.Val("Abba"));
}


TEST(ConstCall2) {
  FunctionTester T("(function(foo,a) { return foo(a,\"3\"); })");
  FunctionTester U("(function (a,b) { return a + b; })");

  T.CheckCall(T.Val("33"), U.function, T.Val(3));
  T.CheckCall(T.Val("3.13"), U.function, T.Val(3.1));
  T.CheckCall(T.Val("function (a,b) { return a + b; }3"), U.function,
              U.function);
  T.CheckCall(T.Val("Abba3"), U.function, T.Val("Abba"));
}


TEST(PropertyNamedCall) {
  FunctionTester T("(function(a,b) { return a.foo(b,23); })");
  CompileRun("function foo(y,z) { return this.x + y + z; }");

  T.CheckCall(T.Val(32), T.NewObject("({ foo:foo, x:4 })"), T.Val(5));
  T.CheckCall(T.Val("xy23"), T.NewObject("({ foo:foo, x:'x' })"), T.Val("y"));
  T.CheckCall(T.nan(), T.NewObject("({ foo:foo, y:0 })"), T.Val(3));
}


TEST(PropertyKeyedCall) {
  FunctionTester T("(function(a,b) { var f = 'foo'; return a[f](b,23); })");
  CompileRun("function foo(y,z) { return this.x + y + z; }");

  T.CheckCall(T.Val(32), T.NewObject("({ foo:foo, x:4 })"), T.Val(5));
  T.CheckCall(T.Val("xy23"), T.NewObject("({ foo:foo, x:'x' })"), T.Val("y"));
  T.CheckCall(T.nan(), T.NewObject("({ foo:foo, y:0 })"), T.Val(3));
}


TEST(GlobalCall) {
  FunctionTester T("(function(a,b) { return foo(a,b); })");
  CompileRun("function foo(a,b) { return a + b + this.c; }");
  CompileRun("var c = 23;");

  T.CheckCall(T.Val(32), T.Val(4), T.Val(5));
  T.CheckCall(T.Val("xy23"), T.Val("x"), T.Val("y"));
  T.CheckCall(T.nan(), T.undefined(), T.Val(3));
}


TEST(LookupCall) {
  FunctionTester T("(function(a,b) { with (a) { return foo(a,b); } })");

  CompileRun("function f1(a,b) { return a.val + b; }");
  T.CheckCall(T.Val(5), T.NewObject("({ foo:f1, val:2 })"), T.Val(3));
  T.CheckCall(T.Val("xy"), T.NewObject("({ foo:f1, val:'x' })"), T.Val("y"));

  CompileRun("function f2(a,b) { return this.val + b; }");
  T.CheckCall(T.Val(9), T.NewObject("({ foo:f2, val:4 })"), T.Val(5));
  T.CheckCall(T.Val("xy"), T.NewObject("({ foo:f2, val:'x' })"), T.Val("y"));
}


TEST(MismatchCallTooFew) {
  FunctionTester T("(function(a,b) { return foo(a,b); })");
  CompileRun("function foo(a,b,c) { return a + b + c; }");

  T.CheckCall(T.nan(), T.Val(23), T.Val(42));
  T.CheckCall(T.nan(), T.Val(4.2), T.Val(2.3));
  T.CheckCall(T.Val("abundefined"), T.Val("a"), T.Val("b"));
}


TEST(MismatchCallTooMany) {
  FunctionTester T("(function(a,b) { return foo(a,b); })");
  CompileRun("function foo(a) { return a; }");

  T.CheckCall(T.Val(23), T.Val(23), T.Val(42));
  T.CheckCall(T.Val(4.2), T.Val(4.2), T.Val(2.3));
  T.CheckCall(T.Val("a"), T.Val("a"), T.Val("b"));
}


TEST(ConstructorCall) {
  FunctionTester T("(function(a,b) { return new foo(a,b).value; })");
  CompileRun("function foo(a,b) { return { value: a + b + this.c }; }");
  CompileRun("foo.prototype.c = 23;");

  T.CheckCall(T.Val(32), T.Val(4), T.Val(5));
  T.CheckCall(T.Val("xy23"), T.Val("x"), T.Val("y"));
  T.CheckCall(T.nan(), T.undefined(), T.Val(3));
}


TEST(RuntimeCall) {
  FLAG_allow_natives_syntax = true;
  FunctionTester T("(function(a) { return %IsJSReceiver(a); })");

  T.CheckCall(T.false_value(), T.Val(23), T.undefined());
  T.CheckCall(T.false_value(), T.Val(4.2), T.undefined());
  T.CheckCall(T.false_value(), T.Val("str"), T.undefined());
  T.CheckCall(T.false_value(), T.true_value(), T.undefined());
  T.CheckCall(T.false_value(), T.false_value(), T.undefined());
  T.CheckCall(T.false_value(), T.undefined(), T.undefined());
  T.CheckCall(T.true_value(), T.NewObject("({})"), T.undefined());
  T.CheckCall(T.true_value(), T.NewObject("([])"), T.undefined());
}


TEST(RuntimeCallInline) {
  FLAG_allow_natives_syntax = true;
  FunctionTester T("(function(a) { return %_IsJSReceiver(a); })");

  T.CheckCall(T.false_value(), T.Val(23), T.undefined());
  T.CheckCall(T.false_value(), T.Val(4.2), T.undefined());
  T.CheckCall(T.false_value(), T.Val("str"), T.undefined());
  T.CheckCall(T.false_value(), T.true_value(), T.undefined());
  T.CheckCall(T.false_value(), T.false_value(), T.undefined());
  T.CheckCall(T.false_value(), T.undefined(), T.undefined());
  T.CheckCall(T.true_value(), T.NewObject("({})"), T.undefined());
  T.CheckCall(T.true_value(), T.NewObject("([])"), T.undefined());
}


TEST(EvalCall) {
  FunctionTester T("(function(a,b) { return eval(a); })");
  Handle<JSObject> g(T.function->context().global_object().global_proxy(),
                     T.isolate);

  T.CheckCall(T.Val(23), T.Val("17 + 6"), T.undefined());
  T.CheckCall(T.Val("'Y'; a"), T.Val("'Y'; a"), T.Val("b-val"));
  T.CheckCall(T.Val("b-val"), T.Val("'Y'; b"), T.Val("b-val"));
  T.CheckCall(g, T.Val("this"), T.undefined());
  T.CheckCall(g, T.Val("'use strict'; this"), T.undefined());

  CompileRun("eval = function(x) { return x; }");
  T.CheckCall(T.Val("17 + 6"), T.Val("17 + 6"), T.undefined());

  CompileRun("eval = function(x) { return this; }");
  T.CheckCall(g, T.Val("17 + 6"), T.undefined());

  CompileRun("eval = function(x) { 'use strict'; return this; }");
  T.CheckCall(T.undefined(), T.Val("17 + 6"), T.undefined());
}


TEST(ReceiverPatching) {
  // TODO(turbofan): Note that this test only checks that the function prologue
  // patches an undefined receiver to the global receiver. If this starts to
  // fail once we fix the calling protocol, just remove this test.
  FunctionTester T("(function(a) { return this; })");
  Handle<JSObject> g(T.function->context().global_object().global_proxy(),
                     T.isolate);
  T.CheckCall(g, T.undefined());
}


TEST(CallEval) {
  FunctionTester T(
      "var x = 42;"
      "(function () {"
      "function bar() { return eval('x') };"
      "return bar;"
      "})();");

  T.CheckCall(T.Val(42), T.Val("x"), T.undefined());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
