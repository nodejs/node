// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects-inl.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

TEST(Conditional) {
  FunctionTester T("(function(a) { return a ? 23 : 42; })");

  T.CheckCall(T.Val(23), T.true_value(), T.undefined());
  T.CheckCall(T.Val(42), T.false_value(), T.undefined());
  T.CheckCall(T.Val(42), T.undefined(), T.undefined());
  T.CheckCall(T.Val(42), T.Val(0.0), T.undefined());
  T.CheckCall(T.Val(23), T.Val(999), T.undefined());
  T.CheckCall(T.Val(23), T.Val("x"), T.undefined());
}


TEST(LogicalAnd) {
  FunctionTester T("(function(a,b) { return a && b; })");

  T.CheckCall(T.true_value(), T.true_value(), T.true_value());
  T.CheckCall(T.false_value(), T.false_value(), T.true_value());
  T.CheckCall(T.false_value(), T.true_value(), T.false_value());
  T.CheckCall(T.false_value(), T.false_value(), T.false_value());

  T.CheckCall(T.Val(999), T.Val(777), T.Val(999));
  T.CheckCall(T.Val(0.0), T.Val(0.0), T.Val(999));
  T.CheckCall(T.Val("b"), T.Val("a"), T.Val("b"));
}


TEST(LogicalOr) {
  FunctionTester T("(function(a,b) { return a || b; })");

  T.CheckCall(T.true_value(), T.true_value(), T.true_value());
  T.CheckCall(T.true_value(), T.false_value(), T.true_value());
  T.CheckCall(T.true_value(), T.true_value(), T.false_value());
  T.CheckCall(T.false_value(), T.false_value(), T.false_value());

  T.CheckCall(T.Val(777), T.Val(777), T.Val(999));
  T.CheckCall(T.Val(999), T.Val(0.0), T.Val(999));
  T.CheckCall(T.Val("a"), T.Val("a"), T.Val("b"));
}


TEST(LogicalEffect) {
  FunctionTester T("(function(a,b) { a && (b = a); return b; })");

  T.CheckCall(T.true_value(), T.true_value(), T.true_value());
  T.CheckCall(T.true_value(), T.false_value(), T.true_value());
  T.CheckCall(T.true_value(), T.true_value(), T.false_value());
  T.CheckCall(T.false_value(), T.false_value(), T.false_value());

  T.CheckCall(T.Val(777), T.Val(777), T.Val(999));
  T.CheckCall(T.Val(999), T.Val(0.0), T.Val(999));
  T.CheckCall(T.Val("a"), T.Val("a"), T.Val("b"));
}


TEST(IfStatement) {
  FunctionTester T("(function(a) { if (a) { return 1; } else { return 2; } })");

  T.CheckCall(T.Val(1), T.true_value(), T.undefined());
  T.CheckCall(T.Val(2), T.false_value(), T.undefined());
  T.CheckCall(T.Val(2), T.undefined(), T.undefined());
  T.CheckCall(T.Val(2), T.Val(0.0), T.undefined());
  T.CheckCall(T.Val(1), T.Val(999), T.undefined());
  T.CheckCall(T.Val(1), T.Val("x"), T.undefined());
}


TEST(DoWhileStatement) {
  FunctionTester T("(function(a,b) { do { a+=23; } while(a < b) return a; })");

  T.CheckCall(T.Val(24), T.Val(1), T.Val(1));
  T.CheckCall(T.Val(24), T.Val(1), T.Val(23));
  T.CheckCall(T.Val(47), T.Val(1), T.Val(25));
  T.CheckCall(T.Val("str23"), T.Val("str"), T.Val("str"));
}


TEST(WhileStatement) {
  FunctionTester T("(function(a,b) { while(a < b) { a+=23; } return a; })");

  T.CheckCall(T.Val(1), T.Val(1), T.Val(1));
  T.CheckCall(T.Val(24), T.Val(1), T.Val(23));
  T.CheckCall(T.Val(47), T.Val(1), T.Val(25));
  T.CheckCall(T.Val("str"), T.Val("str"), T.Val("str"));
}


TEST(ForStatement) {
  FunctionTester T("(function(a,b) { for (; a < b; a+=23) {} return a; })");

  T.CheckCall(T.Val(1), T.Val(1), T.Val(1));
  T.CheckCall(T.Val(24), T.Val(1), T.Val(23));
  T.CheckCall(T.Val(47), T.Val(1), T.Val(25));
  T.CheckCall(T.Val("str"), T.Val("str"), T.Val("str"));
}


static void TestForIn(const char* code) {
  FunctionTester T(code);
  T.CheckCall(T.undefined(), T.undefined());
  T.CheckCall(T.undefined(), T.null());
  T.CheckCall(T.undefined(), T.NewObject("({})"));
  T.CheckCall(T.undefined(), T.Val(1));
  T.CheckCall(T.Val("2"), T.Val("str"));
  T.CheckCall(T.Val("a"), T.NewObject("({'a' : 1})"));
  T.CheckCall(T.Val("2"), T.NewObject("([1, 2, 3])"));
  T.CheckCall(T.Val("a"), T.NewObject("({'a' : 1, 'b' : 1})"), T.Val("b"));
  T.CheckCall(T.Val("1"), T.NewObject("([1, 2, 3])"), T.Val("2"));
}


TEST(ForInStatement) {
  // Variable assignment.
  TestForIn(
      "(function(a, b) {"
      "var last;"
      "for (var x in a) {"
      "  if (b) { delete a[b]; b = undefined; }"
      "  last = x;"
      "}"
      "return last;})");
  // Indexed assignment.
  TestForIn(
      "(function(a, b) {"
      "var array = [0, 1, undefined];"
      "for (array[2] in a) {"
      "  if (b) { delete a[b]; b = undefined; }"
      "}"
      "return array[2];})");
  // Named assignment.
  TestForIn(
      "(function(a, b) {"
      "var obj = {'a' : undefined};"
      "for (obj.a in a) {"
      "  if (b) { delete a[b]; b = undefined; }"
      "}"
      "return obj.a;})");
}


TEST(ForInContinueStatement) {
  const char* src =
      "(function(a,b) {"
      "  var r = '-';"
      "  for (var x in a) {"
      "    r += 'A-';"
      "    if (b) continue;"
      "    r += 'B-';"
      "  }"
      "  return r;"
      "})";
  FunctionTester T(src);

  T.CheckCall(T.Val("-A-B-"), T.NewObject("({x:1})"), T.false_value());
  T.CheckCall(T.Val("-A-B-A-B-"), T.NewObject("({x:1,y:2})"), T.false_value());
  T.CheckCall(T.Val("-A-"), T.NewObject("({x:1})"), T.true_value());
  T.CheckCall(T.Val("-A-A-"), T.NewObject("({x:1,y:2})"), T.true_value());
}


TEST(ForOfContinueStatement) {
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
  FunctionTester T(src);

  CompileRun(
      "function wrap(v) {"
      "  var iterable = {};"
      "  function next() { return { done:!v.length, value:v.shift() }; };"
      "  iterable[Symbol.iterator] = function() { return { next:next }; };"
      "  return iterable;"
      "}");

  T.CheckCall(T.Val("-"), T.NewObject("wrap([])"), T.true_value());
  T.CheckCall(T.Val("-1-2-"), T.NewObject("wrap([1,2])"), T.true_value());
  T.CheckCall(T.Val("-1-X-2-X-"), T.NewObject("wrap([1,2])"), T.false_value());
}


TEST(SwitchStatement) {
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
  FunctionTester T(src);

  T.CheckCall(T.Val("-X-B-D-Y-"), T.Val("x"), T.Val("B"));
  T.CheckCall(T.Val("-B-D-Y-"), T.Val("Bb"), T.Val("B"));
  T.CheckCall(T.Val("-D-Y-"), T.Val("z"), T.Val("B"));
  T.CheckCall(T.Val("-Y-"), T.Val("y"), T.Val("B"));

  CompileRun("var c = 0; var o = { toString:function(){return c++} };");
  T.CheckCall(T.Val("-D-Y-"), T.Val("1b"), T.NewObject("o"));
  T.CheckCall(T.Val("-B-D-Y-"), T.Val("1b"), T.NewObject("o"));
  T.CheckCall(T.Val("-D-Y-"), T.Val("1b"), T.NewObject("o"));
}


TEST(BlockBreakStatement) {
  FunctionTester T("(function(a,b) { L:{ if (a) break L; b=1; } return b; })");

  T.CheckCall(T.Val(7), T.true_value(), T.Val(7));
  T.CheckCall(T.Val(1), T.false_value(), T.Val(7));
}


TEST(BlockReturnStatement) {
  FunctionTester T("(function(a,b) { L:{ if (a) b=1; return b; } })");

  T.CheckCall(T.Val(1), T.true_value(), T.Val(7));
  T.CheckCall(T.Val(7), T.false_value(), T.Val(7));
}


TEST(NestedIfConditional) {
  FunctionTester T("(function(a,b) { if (a) { b = (b?b:7) + 1; } return b; })");

  T.CheckCall(T.Val(4), T.false_value(), T.Val(4));
  T.CheckCall(T.Val(6), T.true_value(), T.Val(5));
  T.CheckCall(T.Val(8), T.true_value(), T.undefined());
}


TEST(NestedIfLogical) {
  const char* src =
      "(function(a,b) {"
      "  if (a || b) { return 1; } else { return 2; }"
      "})";
  FunctionTester T(src);

  T.CheckCall(T.Val(1), T.true_value(), T.true_value());
  T.CheckCall(T.Val(1), T.false_value(), T.true_value());
  T.CheckCall(T.Val(1), T.true_value(), T.false_value());
  T.CheckCall(T.Val(2), T.false_value(), T.false_value());
  T.CheckCall(T.Val(1), T.Val(1.0), T.Val(1.0));
  T.CheckCall(T.Val(1), T.Val(0.0), T.Val(1.0));
  T.CheckCall(T.Val(1), T.Val(1.0), T.Val(0.0));
  T.CheckCall(T.Val(2), T.Val(0.0), T.Val(0.0));
}


TEST(NestedIfElseFor) {
  const char* src =
      "(function(a,b) {"
      "  if (!a) { return b - 3; } else { for (; a < b; a++); }"
      "  return a;"
      "})";
  FunctionTester T(src);

  T.CheckCall(T.Val(1), T.false_value(), T.Val(4));
  T.CheckCall(T.Val(2), T.true_value(), T.Val(2));
  T.CheckCall(T.Val(3), T.Val(3), T.Val(1));
}


TEST(NestedWhileWhile) {
  const char* src =
      "(function(a) {"
      "  var i = a; while (false) while(false) return i;"
      "  return i;"
      "})";
  FunctionTester T(src);

  T.CheckCall(T.Val(2.0), T.Val(2.0), T.Val(-1.0));
  T.CheckCall(T.Val(65.0), T.Val(65.0), T.Val(-1.0));
}


TEST(NestedForIf) {
  FunctionTester T("(function(a,b) { for (; a > 1; a--) if (b) return 1; })");

  T.CheckCall(T.Val(1), T.Val(3), T.true_value());
  T.CheckCall(T.undefined(), T.Val(2), T.false_value());
  T.CheckCall(T.undefined(), T.Val(1), T.null());
}


TEST(NestedForConditional) {
  FunctionTester T("(function(a,b) { for (; a > 1; a--) return b ? 1 : 2; })");

  T.CheckCall(T.Val(1), T.Val(3), T.true_value());
  T.CheckCall(T.Val(2), T.Val(2), T.false_value());
  T.CheckCall(T.undefined(), T.Val(1), T.null());
}


TEST(IfTrue) {
  FunctionTester T("(function(a,b) { if (true) return a; return b; })");

  T.CheckCall(T.Val(55), T.Val(55), T.Val(11));
  T.CheckCall(T.Val(666), T.Val(666), T.Val(-444));
}


TEST(TernaryTrue) {
  FunctionTester T("(function(a,b) { return true ? a : b; })");

  T.CheckCall(T.Val(77), T.Val(77), T.Val(11));
  T.CheckCall(T.Val(111), T.Val(111), T.Val(-444));
}


TEST(IfFalse) {
  FunctionTester T("(function(a,b) { if (false) return a; return b; })");

  T.CheckCall(T.Val(11), T.Val(22), T.Val(11));
  T.CheckCall(T.Val(-555), T.Val(333), T.Val(-555));
}


TEST(TernaryFalse) {
  FunctionTester T("(function(a,b) { return false ? a : b; })");

  T.CheckCall(T.Val(99), T.Val(33), T.Val(99));
  T.CheckCall(T.Val(-99), T.Val(-33), T.Val(-99));
}


TEST(WhileTrue) {
  FunctionTester T("(function(a,b) { while (true) return a; return b; })");

  T.CheckCall(T.Val(551), T.Val(551), T.Val(111));
  T.CheckCall(T.Val(661), T.Val(661), T.Val(-444));
}


TEST(WhileFalse) {
  FunctionTester T("(function(a,b) { while (false) return a; return b; })");

  T.CheckCall(T.Val(115), T.Val(551), T.Val(115));
  T.CheckCall(T.Val(-445), T.Val(661), T.Val(-445));
}


TEST(DoWhileTrue) {
  FunctionTester T(
      "(function(a,b) { do { return a; } while (true); return b; })");

  T.CheckCall(T.Val(7551), T.Val(7551), T.Val(7111));
  T.CheckCall(T.Val(7661), T.Val(7661), T.Val(-7444));
}


TEST(DoWhileFalse) {
  FunctionTester T(
      "(function(a,b) { do { "
      "; } while (false); return b; })");

  T.CheckCall(T.Val(8115), T.Val(8551), T.Val(8115));
  T.CheckCall(T.Val(-8445), T.Val(8661), T.Val(-8445));
}


TEST(EmptyFor) {
  FunctionTester T("(function(a,b) { if (a) for(;;) ; return b; })");

  T.CheckCall(T.Val(8126.1), T.Val(0.0), T.Val(8126.1));
  T.CheckCall(T.Val(1123.1), T.Val(0.0), T.Val(1123.1));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
