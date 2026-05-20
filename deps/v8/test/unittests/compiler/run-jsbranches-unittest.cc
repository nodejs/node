// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/objects-inl.h"
#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

using RunJSBranchesTest = TestWithContext;

TEST_F(RunJSBranchesTest, Conditional) {
  FunctionTester T(i_isolate(), "(function(a) { return a ? 23 : 42; })");

  T.CheckCall(T.NewNumber(23), T.true_value(), T.undefined());
  T.CheckCall(T.NewNumber(42), T.false_value(), T.undefined());
  T.CheckCall(T.NewNumber(42), T.undefined(), T.undefined());
  T.CheckCall(T.NewNumber(42), T.NewNumber(0.0), T.undefined());
  T.CheckCall(T.NewNumber(23), T.NewNumber(999), T.undefined());
  T.CheckCall(T.NewNumber(23), T.NewString("x"), T.undefined());
}

TEST_F(RunJSBranchesTest, LogicalAnd) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a && b; })");

  T.CheckCall(T.true_value(), T.true_value(), T.true_value());
  T.CheckCall(T.false_value(), T.false_value(), T.true_value());
  T.CheckCall(T.false_value(), T.true_value(), T.false_value());
  T.CheckCall(T.false_value(), T.false_value(), T.false_value());

  T.CheckCall(T.NewNumber(999), T.NewNumber(777), T.NewNumber(999));
  T.CheckCall(T.NewNumber(0.0), T.NewNumber(0.0), T.NewNumber(999));
  T.CheckCall(T.NewString("b"), T.NewString("a"), T.NewString("b"));
}

TEST_F(RunJSBranchesTest, LogicalOr) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a || b; })");

  T.CheckCall(T.true_value(), T.true_value(), T.true_value());
  T.CheckCall(T.true_value(), T.false_value(), T.true_value());
  T.CheckCall(T.true_value(), T.true_value(), T.false_value());
  T.CheckCall(T.false_value(), T.false_value(), T.false_value());

  T.CheckCall(T.NewNumber(777), T.NewNumber(777), T.NewNumber(999));
  T.CheckCall(T.NewNumber(999), T.NewNumber(0.0), T.NewNumber(999));
  T.CheckCall(T.NewString("a"), T.NewString("a"), T.NewString("b"));
}

TEST_F(RunJSBranchesTest, LogicalEffect) {
  FunctionTester T(i_isolate(), "(function(a,b) { a && (b = a); return b; })");

  T.CheckCall(T.true_value(), T.true_value(), T.true_value());
  T.CheckCall(T.true_value(), T.false_value(), T.true_value());
  T.CheckCall(T.true_value(), T.true_value(), T.false_value());
  T.CheckCall(T.false_value(), T.false_value(), T.false_value());

  T.CheckCall(T.NewNumber(777), T.NewNumber(777), T.NewNumber(999));
  T.CheckCall(T.NewNumber(999), T.NewNumber(0.0), T.NewNumber(999));
  T.CheckCall(T.NewString("a"), T.NewString("a"), T.NewString("b"));
}

TEST_F(RunJSBranchesTest, IfStatement) {
  FunctionTester T(i_isolate(),
                   "(function(a) { if (a) { return 1; } else { return 2; } })");

  T.CheckCall(T.NewNumber(1), T.true_value(), T.undefined());
  T.CheckCall(T.NewNumber(2), T.false_value(), T.undefined());
  T.CheckCall(T.NewNumber(2), T.undefined(), T.undefined());
  T.CheckCall(T.NewNumber(2), T.NewNumber(0.0), T.undefined());
  T.CheckCall(T.NewNumber(1), T.NewNumber(999), T.undefined());
  T.CheckCall(T.NewNumber(1), T.NewString("x"), T.undefined());
}

TEST_F(RunJSBranchesTest, DoWhileStatement) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { do { a+=23; } while(a < b) return a; })");

  T.CheckCall(T.NewNumber(24), T.NewNumber(1), T.NewNumber(1));
  T.CheckCall(T.NewNumber(24), T.NewNumber(1), T.NewNumber(23));
  T.CheckCall(T.NewNumber(47), T.NewNumber(1), T.NewNumber(25));
  T.CheckCall(T.NewString("str23"), T.NewString("str"), T.NewString("str"));
}

TEST_F(RunJSBranchesTest, WhileStatement) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { while(a < b) { a+=23; } return a; })");

  T.CheckCall(T.NewNumber(1), T.NewNumber(1), T.NewNumber(1));
  T.CheckCall(T.NewNumber(24), T.NewNumber(1), T.NewNumber(23));
  T.CheckCall(T.NewNumber(47), T.NewNumber(1), T.NewNumber(25));
  T.CheckCall(T.NewString("str"), T.NewString("str"), T.NewString("str"));
}

TEST_F(RunJSBranchesTest, ForStatement) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { for (; a < b; a+=23) {} return a; })");

  T.CheckCall(T.NewNumber(1), T.NewNumber(1), T.NewNumber(1));
  T.CheckCall(T.NewNumber(24), T.NewNumber(1), T.NewNumber(23));
  T.CheckCall(T.NewNumber(47), T.NewNumber(1), T.NewNumber(25));
  T.CheckCall(T.NewString("str"), T.NewString("str"), T.NewString("str"));
}

TEST_F(RunJSBranchesTest, ForOfContinueStatement) {
  const char* src =
      "(function(a,b) {"
      "  var r = '-';"
      "  for (var x of a) {"
      "    r += x + '-';"
      "    if (b) continue;"
      "    r += 'X-';"
      "  }"
      "  return r;"
      "})";
  FunctionTester T(i_isolate(), src);

  TryRunJS(
      "function wrap(v) {"
      "  var iterable = {};"
      "  function next() { return { done:!v.length, value:v.shift() }; };"
      "  iterable[Symbol.iterator] = function() { return { next:next }; };"
      "  return iterable;"
      "}");

  T.CheckCall(T.NewString("-"), T.NewObject("wrap([])"), T.true_value());
  T.CheckCall(T.NewString("-1-2-"), T.NewObject("wrap([1,2])"), T.true_value());
  T.CheckCall(T.NewString("-1-X-2-X-"), T.NewObject("wrap([1,2])"),
              T.false_value());
}

TEST_F(RunJSBranchesTest, SwitchStatement) {
  const char* src =
      "(function(a,b) {"
      "  var r = '-';"
      "  switch (a) {"
      "    case 'x'    : r += 'X-';"
      "    case b + 'b': r += 'B-';"
      "    default     : r += 'D-';"
      "    case 'y'    : r += 'Y-';"
      "  }"
      "  return r;"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewString("-X-B-D-Y-"), T.NewString("x"), T.NewString("B"));
  T.CheckCall(T.NewString("-B-D-Y-"), T.NewString("Bb"), T.NewString("B"));
  T.CheckCall(T.NewString("-D-Y-"), T.NewString("z"), T.NewString("B"));
  T.CheckCall(T.NewString("-Y-"), T.NewString("y"), T.NewString("B"));

  TryRunJS("var c = 0; var o = { toString:function(){return c++} };");
  T.CheckCall(T.NewString("-D-Y-"), T.NewString("1b"), T.NewObject("o"));
  T.CheckCall(T.NewString("-B-D-Y-"), T.NewString("1b"), T.NewObject("o"));
  T.CheckCall(T.NewString("-D-Y-"), T.NewString("1b"), T.NewObject("o"));
}

TEST_F(RunJSBranchesTest, BlockBreakStatement) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { L:{ if (a) break L; b=1; } return b; })");

  T.CheckCall(T.NewNumber(7), T.true_value(), T.NewNumber(7));
  T.CheckCall(T.NewNumber(1), T.false_value(), T.NewNumber(7));
}

TEST_F(RunJSBranchesTest, BlockReturnStatement) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { L:{ if (a) b=1; return b; } })");

  T.CheckCall(T.NewNumber(1), T.true_value(), T.NewNumber(7));
  T.CheckCall(T.NewNumber(7), T.false_value(), T.NewNumber(7));
}

TEST_F(RunJSBranchesTest, NestedIfConditional) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { if (a) { b = (b?b:7) + 1; } return b; })");

  T.CheckCall(T.NewNumber(4), T.false_value(), T.NewNumber(4));
  T.CheckCall(T.NewNumber(6), T.true_value(), T.NewNumber(5));
  T.CheckCall(T.NewNumber(8), T.true_value(), T.undefined());
}

TEST_F(RunJSBranchesTest, NestedIfLogical) {
  const char* src =
      "(function(a,b) {"
      "  if (a || b) { return 1; } else { return 2; }"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewNumber(1), T.true_value(), T.true_value());
  T.CheckCall(T.NewNumber(1), T.false_value(), T.true_value());
  T.CheckCall(T.NewNumber(1), T.true_value(), T.false_value());
  T.CheckCall(T.NewNumber(2), T.false_value(), T.false_value());
  T.CheckCall(T.NewNumber(1), T.NewNumber(1.0), T.NewNumber(1.0));
  T.CheckCall(T.NewNumber(1), T.NewNumber(0.0), T.NewNumber(1.0));
  T.CheckCall(T.NewNumber(1), T.NewNumber(1.0), T.NewNumber(0.0));
  T.CheckCall(T.NewNumber(2), T.NewNumber(0.0), T.NewNumber(0.0));
}

TEST_F(RunJSBranchesTest, NestedIfElseFor) {
  const char* src =
      "(function(a,b) {"
      "  if (!a) { return b - 3; } else { for (; a < b; a++); }"
      "  return a;"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewNumber(1), T.false_value(), T.NewNumber(4));
  T.CheckCall(T.NewNumber(2), T.true_value(), T.NewNumber(2));
  T.CheckCall(T.NewNumber(3), T.NewNumber(3), T.NewNumber(1));
}

TEST_F(RunJSBranchesTest, NestedWhileWhile) {
  const char* src =
      "(function(a) {"
      "  var i = a; while (false) while(false) return i;"
      "  return i;"
      "})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewNumber(2.0), T.NewNumber(2.0), T.NewNumber(-1.0));
  T.CheckCall(T.NewNumber(65.0), T.NewNumber(65.0), T.NewNumber(-1.0));
}

TEST_F(RunJSBranchesTest, NestedForIf) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { for (; a > 1; a--) if (b) return 1; })");

  T.CheckCall(T.NewNumber(1), T.NewNumber(3), T.true_value());
  T.CheckCall(T.undefined(), T.NewNumber(2), T.false_value());
  T.CheckCall(T.undefined(), T.NewNumber(1), T.null());
}

TEST_F(RunJSBranchesTest, NestedForConditional) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { for (; a > 1; a--) return b ? 1 : 2; })");

  T.CheckCall(T.NewNumber(1), T.NewNumber(3), T.true_value());
  T.CheckCall(T.NewNumber(2), T.NewNumber(2), T.false_value());
  T.CheckCall(T.undefined(), T.NewNumber(1), T.null());
}

TEST_F(RunJSBranchesTest, IfTrue) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { if (true) return a; return b; })");

  T.CheckCall(T.NewNumber(55), T.NewNumber(55), T.NewNumber(11));
  T.CheckCall(T.NewNumber(666), T.NewNumber(666), T.NewNumber(-444));
}

TEST_F(RunJSBranchesTest, TernaryTrue) {
  FunctionTester T(i_isolate(), "(function(a,b) { return true ? a : b; })");

  T.CheckCall(T.NewNumber(77), T.NewNumber(77), T.NewNumber(11));
  T.CheckCall(T.NewNumber(111), T.NewNumber(111), T.NewNumber(-444));
}

TEST_F(RunJSBranchesTest, IfFalse) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { if (false) return a; return b; })");

  T.CheckCall(T.NewNumber(11), T.NewNumber(22), T.NewNumber(11));
  T.CheckCall(T.NewNumber(-555), T.NewNumber(333), T.NewNumber(-555));
}

TEST_F(RunJSBranchesTest, TernaryFalse) {
  FunctionTester T(i_isolate(), "(function(a,b) { return false ? a : b; })");

  T.CheckCall(T.NewNumber(99), T.NewNumber(33), T.NewNumber(99));
  T.CheckCall(T.NewNumber(-99), T.NewNumber(-33), T.NewNumber(-99));
}

TEST_F(RunJSBranchesTest, WhileTrue) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { while (true) return a; return b; })");

  T.CheckCall(T.NewNumber(551), T.NewNumber(551), T.NewNumber(111));
  T.CheckCall(T.NewNumber(661), T.NewNumber(661), T.NewNumber(-444));
}

TEST_F(RunJSBranchesTest, WhileFalse) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { while (false) return a; return b; })");

  T.CheckCall(T.NewNumber(115), T.NewNumber(551), T.NewNumber(115));
  T.CheckCall(T.NewNumber(-445), T.NewNumber(661), T.NewNumber(-445));
}

TEST_F(RunJSBranchesTest, DoWhileTrue) {
  FunctionTester T(
      i_isolate(),
      "(function(a,b) { do { return a; } while (true); return b; })");

  T.CheckCall(T.NewNumber(7551), T.NewNumber(7551), T.NewNumber(7111));
  T.CheckCall(T.NewNumber(7661), T.NewNumber(7661), T.NewNumber(-7444));
}

TEST_F(RunJSBranchesTest, DoWhileFalse) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { do { "
                   "; } while (false); return b; })");

  T.CheckCall(T.NewNumber(8115), T.NewNumber(8551), T.NewNumber(8115));
  T.CheckCall(T.NewNumber(-8445), T.NewNumber(8661), T.NewNumber(-8445));
}

TEST_F(RunJSBranchesTest, EmptyFor) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { if (a) for(;;) ; return b; })");

  T.CheckCall(T.NewNumber(8126.1), T.NewNumber(0.0), T.NewNumber(8126.1));
  T.CheckCall(T.NewNumber(1123.1), T.NewNumber(0.0), T.NewNumber(1123.1));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
