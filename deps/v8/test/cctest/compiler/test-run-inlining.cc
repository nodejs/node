// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "test/cctest/compiler/function-tester.h"

#if V8_TURBOFAN_TARGET

using namespace v8::internal;
using namespace v8::internal::compiler;

namespace {

// Helper to determine inline count via JavaScriptFrame::GetInlineCount.
// Note that a count of 1 indicates that no inlining has occured.
void AssertInlineCount(const v8::FunctionCallbackInfo<v8::Value>& args) {
  StackTraceFrameIterator it(CcTest::i_isolate());
  int frames_seen = 0;
  JavaScriptFrame* topmost = it.frame();
  while (!it.done()) {
    JavaScriptFrame* frame = it.frame();
    PrintF("%d %s, inline count: %d\n", frames_seen,
           frame->function()->shared()->DebugName()->ToCString().get(),
           frame->GetInlineCount());
    frames_seen++;
    it.Advance();
  }
  CHECK_EQ(args[0]->ToInt32(args.GetIsolate())->Value(),
           topmost->GetInlineCount());
}


void InstallAssertInlineCountHelper(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::FunctionTemplate> t =
      v8::FunctionTemplate::New(isolate, AssertInlineCount);
  context->Global()->Set(v8_str("AssertInlineCount"), t->GetFunction());
}


const uint32_t kBuiltinInlineFlags = CompilationInfo::kBuiltinInliningEnabled |
                                     CompilationInfo::kContextSpecializing |
                                     CompilationInfo::kTypingEnabled;

const uint32_t kInlineFlags = CompilationInfo::kInliningEnabled |
                              CompilationInfo::kContextSpecializing |
                              CompilationInfo::kTypingEnabled;

}  // namespace


TEST(SimpleInlining) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function(){"
      "  function foo(s) { AssertInlineCount(2); return s; };"
      "  function bar(s, t) { return foo(s); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(1), T.Val(1), T.Val(2));
}


TEST(SimpleInliningDeopt) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function(){"
      "  function foo(s) { %DeoptimizeFunction(bar); return s; };"
      "  function bar(s, t) { return foo(s); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(1), T.Val(1), T.Val(2));
}


TEST(SimpleInliningContext) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s) { AssertInlineCount(2); var x = 12; return s + x; };"
      "  function bar(s, t) { return foo(s); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(13), T.Val(1), T.Val(2));
}


TEST(SimpleInliningContextDeopt) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s) {"
      "    AssertInlineCount(2); %DeoptimizeFunction(bar); var x = 12;"
      "    return s + x;"
      "  };"
      "  function bar(s, t) { return foo(s); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(13), T.Val(1), T.Val(2));
}


TEST(CaptureContext) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "var f = (function () {"
      "  var x = 42;"
      "  function bar(s) { return x + s; };"
      "  return (function (s) { return bar(s); });"
      "})();"
      "(function (s) { return f(s) })",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42 + 12), T.Val(12), T.undefined());
}


// TODO(sigurds) For now we do not inline any native functions. If we do at
// some point, change this test.
TEST(DontInlineEval) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "var x = 42;"
      "(function () {"
      "  function bar(s, t) { return eval(\"AssertInlineCount(1); x\") };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42), T.Val("x"), T.undefined());
}


TEST(InlineOmitArguments) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = 42;"
      "  function bar(s, t, u, v) { AssertInlineCount(2); return x + s; };"
      "  return (function (s,t) { return bar(s); });"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42 + 12), T.Val(12), T.undefined());
}


TEST(InlineOmitArgumentsDeopt) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s,t,u,v) { AssertInlineCount(2);"
      "                          %DeoptimizeFunction(bar); return baz(); };"
      "  function bar() { return foo(11); };"
      "  function baz() { return foo.arguments.length == 1 &&"
      "                          foo.arguments[0] == 11; }"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(12), T.Val(14));
}


TEST(InlineSurplusArguments) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = 42;"
      "  function foo(s) { AssertInlineCount(2); return x + s; };"
      "  function bar(s,t) { return foo(s,t,13); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42 + 12), T.Val(12), T.undefined());
}


TEST(InlineSurplusArgumentsDeopt) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s) { AssertInlineCount(2); %DeoptimizeFunction(bar);"
      "                    return baz(); };"
      "  function bar() { return foo(13, 14, 15); };"
      "  function baz() { return foo.arguments.length == 3 &&"
      "                          foo.arguments[0] == 13 &&"
      "                          foo.arguments[1] == 14 &&"
      "                          foo.arguments[2] == 15; }"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(12), T.Val(14));
}


TEST(InlineTwice) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = 42;"
      "  function bar(s) { AssertInlineCount(2); return x + s; };"
      "  return (function (s,t) { return bar(s) + bar(t); });"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(2 * 42 + 12 + 4), T.Val(12), T.Val(4));
}


TEST(InlineTwiceDependent) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = 42;"
      "  function foo(s) { AssertInlineCount(2); return x + s; };"
      "  function bar(s,t) { return foo(foo(s)); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42 + 42 + 12), T.Val(12), T.Val(4));
}


TEST(InlineTwiceDependentDiamond) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = 41;"
      "  function foo(s) { AssertInlineCount(2); if (s % 2 == 0) {"
      "                    return x - s } else { return x + s; } };"
      "  function bar(s,t) { return foo(foo(s)); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(-11), T.Val(11), T.Val(4));
}


TEST(InlineTwiceDependentDiamondDifferent) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = 41;"
      "  function foo(s,t) { AssertInlineCount(2); if (s % 2 == 0) {"
      "                      return x - s * t } else { return x + s * t; } };"
      "  function bar(s,t) { return foo(foo(s, 3), 5); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(-329), T.Val(11), T.Val(4));
}


TEST(InlineLoopGuardedEmpty) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s) { AssertInlineCount(2); if (s) while (s); return s; };"
      "  function bar(s,t) { return foo(s); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(0.0), T.Val(4));
}


TEST(InlineLoopGuardedOnce) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s,t) { AssertInlineCount(2); if (t > 0) while (s > 0) {"
      "                      s = s - 1; }; return s; };"
      "  function bar(s,t) { return foo(s,t); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(11), T.Val(4));
}


TEST(InlineLoopGuardedTwice) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s,t) { AssertInlineCount(2); if (t > 0) while (s > 0) {"
      "                      s = s - 1; }; return s; };"
      "  function bar(s,t) { return foo(foo(s,t),t); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(11), T.Val(4));
}


TEST(InlineLoopUnguardedEmpty) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s) { AssertInlineCount(2); while (s); return s; };"
      "  function bar(s, t) { return foo(s); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(0.0), T.Val(4));
}


TEST(InlineLoopUnguardedOnce) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s) { AssertInlineCount(2); while (s) {"
      "                    s = s - 1; }; return s; };"
      "  function bar(s, t) { return foo(s); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(0.0), T.Val(4));
}


TEST(InlineLoopUnguardedTwice) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s) { AssertInlineCount(2); while (s > 0) {"
      "                    s = s - 1; }; return s; };"
      "  function bar(s,t) { return foo(foo(s,t),t); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(0.0), T.Val(4));
}


TEST(InlineStrictIntoNonStrict) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = Object.create({}, { y: { value:42, writable:false } });"
      "  function foo(s) { 'use strict';"
      "                     x.y = 9; };"
      "  function bar(s,t) { return foo(s); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckThrows(T.undefined(), T.undefined());
}


TEST(InlineNonStrictIntoStrict) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = Object.create({}, { y: { value:42, writable:false } });"
      "  function foo(s) { x.y = 9; return x.y; };"
      "  function bar(s,t) { \'use strict\'; return foo(s); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42), T.undefined(), T.undefined());
}


TEST(InlineIntrinsicIsSmi) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = 42;"
      "  function bar(s,t) { return %_IsSmi(x); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(12), T.Val(4));
}


TEST(InlineIntrinsicIsNonNegativeSmi) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = 42;"
      "  function bar(s,t) { return %_IsNonNegativeSmi(x); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(12), T.Val(4));
}


TEST(InlineIntrinsicIsArray) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  var x = [1,2,3];"
      "  function bar(s,t) { return %_IsArray(x); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(12), T.Val(4));

  FunctionTester T2(
      "(function () {"
      "  var x = 32;"
      "  function bar(s,t) { return %_IsArray(x); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  T2.CheckCall(T.false_value(), T.Val(12), T.Val(4));

  FunctionTester T3(
      "(function () {"
      "  var x = bar;"
      "  function bar(s,t) { return %_IsArray(x); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  T3.CheckCall(T.false_value(), T.Val(12), T.Val(4));
}


TEST(InlineWithArguments) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s,t,u) { AssertInlineCount(2);"
      "    return foo.arguments.length == 3 &&"
      "           foo.arguments[0] == 13 &&"
      "           foo.arguments[1] == 14 &&"
      "           foo.arguments[2] == 15;"
      "  }"
      "  function bar() { return foo(13, 14, 15); };"
      "  return bar;"
      "})();",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(12), T.Val(14));
}


TEST(InlineBuiltin) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s,t,u) { AssertInlineCount(2); return true; }"
      "  function bar() { return foo(); };"
      "  %SetInlineBuiltinFlag(foo);"
      "  return bar;"
      "})();",
      kBuiltinInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value());
}


TEST(InlineNestedBuiltin) {
  FLAG_turbo_deoptimization = true;
  FunctionTester T(
      "(function () {"
      "  function foo(s,t,u) { AssertInlineCount(3); return true; }"
      "  function baz(s,t,u) { return foo(s,t,u); }"
      "  function bar() { return baz(); };"
      "  %SetInlineBuiltinFlag(foo);"
      "  %SetInlineBuiltinFlag(baz);"
      "  return bar;"
      "})();",
      kBuiltinInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value());
}

#endif  // V8_TURBOFAN_TARGET
