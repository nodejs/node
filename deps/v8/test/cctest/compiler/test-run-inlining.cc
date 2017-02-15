// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compilation-info.h"
#include "src/frames-inl.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

// Helper to determine inline count via JavaScriptFrame::GetFunctions.
// Note that a count of 1 indicates that no inlining has occured.
void AssertInlineCount(const v8::FunctionCallbackInfo<v8::Value>& args) {
  JavaScriptFrameIterator it(CcTest::i_isolate());
  int frames_seen = 0;
  JavaScriptFrame* topmost = it.frame();
  while (!it.done()) {
    JavaScriptFrame* frame = it.frame();
    List<JSFunction*> functions(2);
    frame->GetFunctions(&functions);
    PrintF("%d %s, inline count: %d\n", frames_seen,
           frame->function()->shared()->DebugName()->ToCString().get(),
           functions.length());
    frames_seen++;
    it.Advance();
  }
  List<JSFunction*> functions(2);
  topmost->GetFunctions(&functions);
  CHECK_EQ(args[0]
               ->ToInt32(args.GetIsolate()->GetCurrentContext())
               .ToLocalChecked()
               ->Value(),
           functions.length());
}


void InstallAssertInlineCountHelper(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::FunctionTemplate> t =
      v8::FunctionTemplate::New(isolate, AssertInlineCount);
  CHECK(context->Global()
            ->Set(context, v8_str("AssertInlineCount"),
                  t->GetFunction(context).ToLocalChecked())
            .FromJust());
}

const uint32_t kRestrictedInliningFlags =
    CompilationInfo::kNativeContextSpecializing;

const uint32_t kInlineFlags = CompilationInfo::kInliningEnabled |
                              CompilationInfo::kNativeContextSpecializing;

}  // namespace


TEST(SimpleInlining) {
  FunctionTester T(
      "function foo(s) { AssertInlineCount(2); return s; };"
      "function bar(s, t) { return foo(s); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(1), T.Val(1), T.Val(2));
}


TEST(SimpleInliningDeopt) {
  FunctionTester T(
      "function foo(s) { %DeoptimizeFunction(bar); return s; };"
      "function bar(s, t) { return foo(s); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(1), T.Val(1), T.Val(2));
}


TEST(SimpleInliningDeoptSelf) {
  FunctionTester T(
      "function foo(s) { %_DeoptimizeNow(); return s; };"
      "function bar(s, t) { return foo(s); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(1), T.Val(1), T.Val(2));
}


TEST(SimpleInliningContext) {
  FunctionTester T(
      "function foo(s) { AssertInlineCount(2); var x = 12; return s + x; };"
      "function bar(s, t) { return foo(s); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(13), T.Val(1), T.Val(2));
}


TEST(SimpleInliningContextDeopt) {
  FunctionTester T(
      "function foo(s) {"
      "  AssertInlineCount(2); %DeoptimizeFunction(bar); var x = 12;"
      "  return s + x;"
      "};"
      "function bar(s, t) { return foo(s); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(13), T.Val(1), T.Val(2));
}


TEST(CaptureContext) {
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
  FunctionTester T(
      "var x = 42;"
      "function bar(s, t) { return eval(\"AssertInlineCount(1); x\") };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42), T.Val("x"), T.undefined());
}


TEST(InlineOmitArguments) {
  FunctionTester T(
      "var x = 42;"
      "function bar(s, t, u, v) { AssertInlineCount(2); return x + s; };"
      "function foo(s, t) { return bar(s); };"
      "foo;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42 + 12), T.Val(12), T.undefined());
}


TEST(InlineOmitArgumentsObject) {
  FunctionTester T(
      "function bar(s, t, u, v) { AssertInlineCount(2); return arguments; };"
      "function foo(s, t) { var args = bar(s);"
      "                     return args.length == 1 &&"
      "                            args[0] == 11; };"
      "foo;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(11), T.undefined());
}


TEST(InlineOmitArgumentsDeopt) {
  FunctionTester T(
      "function foo(s,t,u,v) { AssertInlineCount(2);"
      "                        %DeoptimizeFunction(bar); return baz(); };"
      "function bar() { return foo(11); };"
      "function baz() { return foo.arguments.length == 1 &&"
      "                        foo.arguments[0] == 11; }"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(12), T.Val(14));
}


TEST(InlineSurplusArguments) {
  FunctionTester T(
      "var x = 42;"
      "function foo(s) { AssertInlineCount(2); return x + s; };"
      "function bar(s, t) { return foo(s, t, 13); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42 + 12), T.Val(12), T.undefined());
}


TEST(InlineSurplusArgumentsObject) {
  FunctionTester T(
      "function foo(s) { AssertInlineCount(2); return arguments; };"
      "function bar(s, t) { var args = foo(s, t, 13);"
      "                     return args.length == 3 &&"
      "                            args[0] == 11 &&"
      "                            args[1] == 12 &&"
      "                            args[2] == 13; };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(11), T.Val(12));
}


TEST(InlineSurplusArgumentsDeopt) {
  FunctionTester T(
      "function foo(s) { AssertInlineCount(2); %DeoptimizeFunction(bar);"
      "                  return baz(); };"
      "function bar() { return foo(13, 14, 15); };"
      "function baz() { return foo.arguments.length == 3 &&"
      "                        foo.arguments[0] == 13 &&"
      "                        foo.arguments[1] == 14 &&"
      "                        foo.arguments[2] == 15; }"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(12), T.Val(14));
}


TEST(InlineTwice) {
  FunctionTester T(
      "var x = 42;"
      "function bar(s) { AssertInlineCount(2); return x + s; };"
      "function foo(s, t) { return bar(s) + bar(t); };"
      "foo;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(2 * 42 + 12 + 4), T.Val(12), T.Val(4));
}


TEST(InlineTwiceDependent) {
  FunctionTester T(
      "var x = 42;"
      "function foo(s) { AssertInlineCount(2); return x + s; };"
      "function bar(s,t) { return foo(foo(s)); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42 + 42 + 12), T.Val(12), T.Val(4));
}


TEST(InlineTwiceDependentDiamond) {
  FunctionTester T(
      "var x = 41;"
      "function foo(s) { AssertInlineCount(2); if (s % 2 == 0) {"
      "                  return x - s } else { return x + s; } };"
      "function bar(s,t) { return foo(foo(s)); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(-11), T.Val(11), T.Val(4));
}


TEST(InlineTwiceDependentDiamondDifferent) {
  FunctionTester T(
      "var x = 41;"
      "function foo(s,t) { AssertInlineCount(2); if (s % 2 == 0) {"
      "                    return x - s * t } else { return x + s * t; } };"
      "function bar(s,t) { return foo(foo(s, 3), 5); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(-329), T.Val(11), T.Val(4));
}


TEST(InlineLoopGuardedEmpty) {
  FunctionTester T(
      "function foo(s) { AssertInlineCount(2); if (s) while (s); return s; };"
      "function bar(s,t) { return foo(s); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(0.0), T.Val(4));
}


TEST(InlineLoopGuardedOnce) {
  FunctionTester T(
      "function foo(s,t) { AssertInlineCount(2); if (t > 0) while (s > 0) {"
      "                    s = s - 1; }; return s; };"
      "function bar(s,t) { return foo(s,t); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(11), T.Val(4));
}


TEST(InlineLoopGuardedTwice) {
  FunctionTester T(
      "function foo(s,t) { AssertInlineCount(2); if (t > 0) while (s > 0) {"
      "                    s = s - 1; }; return s; };"
      "function bar(s,t) { return foo(foo(s,t),t); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(11), T.Val(4));
}


TEST(InlineLoopUnguardedEmpty) {
  FunctionTester T(
      "function foo(s) { AssertInlineCount(2); while (s); return s; };"
      "function bar(s, t) { return foo(s); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(0.0), T.Val(4));
}


TEST(InlineLoopUnguardedOnce) {
  FunctionTester T(
      "function foo(s) { AssertInlineCount(2); while (s) {"
      "                  s = s - 1; }; return s; };"
      "function bar(s, t) { return foo(s); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(0.0), T.Val(4));
}


TEST(InlineLoopUnguardedTwice) {
  FunctionTester T(
      "function foo(s) { AssertInlineCount(2); while (s > 0) {"
      "                  s = s - 1; }; return s; };"
      "function bar(s,t) { return foo(foo(s,t),t); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(0.0), T.Val(0.0), T.Val(4));
}


TEST(InlineStrictIntoNonStrict) {
  FunctionTester T(
      "var x = Object.create({}, { y: { value:42, writable:false } });"
      "function foo(s) { 'use strict';"
      "                   x.y = 9; };"
      "function bar(s,t) { return foo(s); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckThrows(T.undefined(), T.undefined());
}


TEST(InlineNonStrictIntoStrict) {
  FunctionTester T(
      "var x = Object.create({}, { y: { value:42, writable:false } });"
      "function foo(s) { x.y = 9; return x.y; };"
      "function bar(s,t) { \'use strict\'; return foo(s); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42), T.undefined(), T.undefined());
}


TEST(InlineWithArguments) {
  FunctionTester T(
      "function foo(s,t,u) { AssertInlineCount(2);"
      "  return foo.arguments.length == 3 &&"
      "         foo.arguments[0] == 13 &&"
      "         foo.arguments[1] == 14 &&"
      "         foo.arguments[2] == 15;"
      "}"
      "function bar() { return foo(13, 14, 15); };"
      "bar;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value(), T.Val(12), T.Val(14));
}


TEST(InlineBuiltin) {
  FunctionTester T(
      "function foo(s,t,u) { AssertInlineCount(2); return true; }"
      "function bar() { return foo(); };"
      "%SetForceInlineFlag(foo);"
      "bar;",
      kRestrictedInliningFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value());
}


TEST(InlineNestedBuiltin) {
  FunctionTester T(
      "function foo(s,t,u) { AssertInlineCount(3); return true; }"
      "function baz(s,t,u) { return foo(s,t,u); }"
      "function bar() { return baz(); };"
      "%SetForceInlineFlag(foo);"
      "%SetForceInlineFlag(baz);"
      "bar;",
      kRestrictedInliningFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.true_value());
}


TEST(InlineSelfRecursive) {
  FunctionTester T(
      "function foo(x) { "
      "  AssertInlineCount(1);"
      "  if (x == 1) return foo(12);"
      "  return x;"
      "}"
      "foo;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(12), T.Val(1));
}


TEST(InlineMutuallyRecursive) {
  FunctionTester T(
      "function bar(x) { AssertInlineCount(2); return foo(x); }"
      "function foo(x) { "
      "  if (x == 1) return bar(42);"
      "  return x;"
      "}"
      "foo;",
      kInlineFlags);

  InstallAssertInlineCountHelper(CcTest::isolate());
  T.CheckCall(T.Val(42), T.Val(1));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
