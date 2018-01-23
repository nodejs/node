// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects-inl.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

TEST(BinopAdd) {
  FunctionTester T("(function(a,b) { return a + b; })");

  T.CheckCall(3, 1, 2);
  T.CheckCall(-11, -2, -9);
  T.CheckCall(-11, -1.5, -9.5);
  T.CheckCall(T.Val("AB"), T.Val("A"), T.Val("B"));
  T.CheckCall(T.Val("A11"), T.Val("A"), T.Val(11));
  T.CheckCall(T.Val("12B"), T.Val(12), T.Val("B"));
  T.CheckCall(T.Val("38"), T.Val("3"), T.Val("8"));
  T.CheckCall(T.Val("31"), T.Val("3"), T.NewObject("([1])"));
  T.CheckCall(T.Val("3[object Object]"), T.Val("3"), T.NewObject("({})"));
}


TEST(BinopSubtract) {
  FunctionTester T("(function(a,b) { return a - b; })");

  T.CheckCall(3, 4, 1);
  T.CheckCall(3.0, 4.5, 1.5);
  T.CheckCall(T.Val(-9), T.Val("0"), T.Val(9));
  T.CheckCall(T.Val(-9), T.Val(0.0), T.Val("9"));
  T.CheckCall(T.Val(1), T.Val("3"), T.Val("2"));
  T.CheckCall(T.nan(), T.Val("3"), T.Val("B"));
  T.CheckCall(T.Val(2), T.Val("3"), T.NewObject("([1])"));
  T.CheckCall(T.nan(), T.Val("3"), T.NewObject("({})"));
}


TEST(BinopMultiply) {
  FunctionTester T("(function(a,b) { return a * b; })");

  T.CheckCall(6, 3, 2);
  T.CheckCall(4.5, 2.0, 2.25);
  T.CheckCall(T.Val(6), T.Val("3"), T.Val(2));
  T.CheckCall(T.Val(4.5), T.Val(2.0), T.Val("2.25"));
  T.CheckCall(T.Val(6), T.Val("3"), T.Val("2"));
  T.CheckCall(T.nan(), T.Val("3"), T.Val("B"));
  T.CheckCall(T.Val(3), T.Val("3"), T.NewObject("([1])"));
  T.CheckCall(T.nan(), T.Val("3"), T.NewObject("({})"));
}


TEST(BinopDivide) {
  FunctionTester T("(function(a,b) { return a / b; })");

  T.CheckCall(2, 8, 4);
  T.CheckCall(2.1, 8.4, 4);
  T.CheckCall(V8_INFINITY, 8, 0);
  T.CheckCall(-V8_INFINITY, -8, 0);
  T.CheckCall(T.infinity(), T.Val(8), T.Val("0"));
  T.CheckCall(T.minus_infinity(), T.Val("-8"), T.Val(0.0));
  T.CheckCall(T.Val(1.5), T.Val("3"), T.Val("2"));
  T.CheckCall(T.nan(), T.Val("3"), T.Val("B"));
  T.CheckCall(T.Val(1.5), T.Val("3"), T.NewObject("([2])"));
  T.CheckCall(T.nan(), T.Val("3"), T.NewObject("({})"));
}


TEST(BinopModulus) {
  FunctionTester T("(function(a,b) { return a % b; })");

  T.CheckCall(3, 8, 5);
  T.CheckCall(T.Val(3), T.Val("8"), T.Val(5));
  T.CheckCall(T.Val(3), T.Val(8), T.Val("5"));
  T.CheckCall(T.Val(1), T.Val("3"), T.Val("2"));
  T.CheckCall(T.nan(), T.Val("3"), T.Val("B"));
  T.CheckCall(T.Val(1), T.Val("3"), T.NewObject("([2])"));
  T.CheckCall(T.nan(), T.Val("3"), T.NewObject("({})"));
}


TEST(BinopShiftLeft) {
  FunctionTester T("(function(a,b) { return a << b; })");

  T.CheckCall(4, 2, 1);
  T.CheckCall(T.Val(4), T.Val("2"), T.Val(1));
  T.CheckCall(T.Val(4), T.Val(2), T.Val("1"));
}


TEST(BinopShiftRight) {
  FunctionTester T("(function(a,b) { return a >> b; })");

  T.CheckCall(4, 8, 1);
  T.CheckCall(-4, -8, 1);
  T.CheckCall(T.Val(4), T.Val("8"), T.Val(1));
  T.CheckCall(T.Val(4), T.Val(8), T.Val("1"));
}


TEST(BinopShiftRightLogical) {
  FunctionTester T("(function(a,b) { return a >>> b; })");

  T.CheckCall(4, 8, 1);
  T.CheckCall(0x7FFFFFFC, -8, 1);
  T.CheckCall(T.Val(4), T.Val("8"), T.Val(1));
  T.CheckCall(T.Val(4), T.Val(8), T.Val("1"));
}


TEST(BinopAnd) {
  FunctionTester T("(function(a,b) { return a & b; })");

  T.CheckCall(7, 7, 15);
  T.CheckCall(7, 15, 7);
  T.CheckCall(T.Val(7), T.Val("15"), T.Val(7));
  T.CheckCall(T.Val(7), T.Val(15), T.Val("7"));
}


TEST(BinopOr) {
  FunctionTester T("(function(a,b) { return a | b; })");

  T.CheckCall(6, 4, 2);
  T.CheckCall(6, 2, 4);
  T.CheckCall(T.Val(6), T.Val("2"), T.Val(4));
  T.CheckCall(T.Val(6), T.Val(2), T.Val("4"));
}


TEST(BinopXor) {
  FunctionTester T("(function(a,b) { return a ^ b; })");

  T.CheckCall(7, 15, 8);
  T.CheckCall(7, 8, 15);
  T.CheckCall(T.Val(7), T.Val("8"), T.Val(15));
  T.CheckCall(T.Val(7), T.Val(8), T.Val("15"));
}


TEST(BinopStrictEqual) {
  FunctionTester T("(function(a,b) { return a === b; })");

  T.CheckTrue(7, 7);
  T.CheckFalse(7, 8);
  T.CheckTrue(7.1, 7.1);
  T.CheckFalse(7.1, 8.1);

  T.CheckTrue(T.Val("7.1"), T.Val("7.1"));
  T.CheckFalse(T.Val(7.1), T.Val("7.1"));
  T.CheckFalse(T.Val(7), T.undefined());
  T.CheckFalse(T.undefined(), T.Val(7));

  CompileRun("var o = { desc : 'I am a singleton' }");
  T.CheckFalse(T.NewObject("([1])"), T.NewObject("([1])"));
  T.CheckFalse(T.NewObject("({})"), T.NewObject("({})"));
  T.CheckTrue(T.NewObject("(o)"), T.NewObject("(o)"));
}


TEST(BinopEqual) {
  FunctionTester T("(function(a,b) { return a == b; })");

  T.CheckTrue(7, 7);
  T.CheckFalse(7, 8);
  T.CheckTrue(7.1, 7.1);
  T.CheckFalse(7.1, 8.1);

  T.CheckTrue(T.Val("7.1"), T.Val("7.1"));
  T.CheckTrue(T.Val(7.1), T.Val("7.1"));

  CompileRun("var o = { desc : 'I am a singleton' }");
  T.CheckFalse(T.NewObject("([1])"), T.NewObject("([1])"));
  T.CheckFalse(T.NewObject("({})"), T.NewObject("({})"));
  T.CheckTrue(T.NewObject("(o)"), T.NewObject("(o)"));
}


TEST(BinopNotEqual) {
  FunctionTester T("(function(a,b) { return a != b; })");

  T.CheckFalse(7, 7);
  T.CheckTrue(7, 8);
  T.CheckFalse(7.1, 7.1);
  T.CheckTrue(7.1, 8.1);

  T.CheckFalse(T.Val("7.1"), T.Val("7.1"));
  T.CheckFalse(T.Val(7.1), T.Val("7.1"));

  CompileRun("var o = { desc : 'I am a singleton' }");
  T.CheckTrue(T.NewObject("([1])"), T.NewObject("([1])"));
  T.CheckTrue(T.NewObject("({})"), T.NewObject("({})"));
  T.CheckFalse(T.NewObject("(o)"), T.NewObject("(o)"));
}


TEST(BinopLessThan) {
  FunctionTester T("(function(a,b) { return a < b; })");

  T.CheckTrue(7, 8);
  T.CheckFalse(8, 7);
  T.CheckTrue(-8.1, -8);
  T.CheckFalse(-8, -8.1);
  T.CheckFalse(0.111, 0.111);

  T.CheckFalse(T.Val("7.1"), T.Val("7.1"));
  T.CheckFalse(T.Val(7.1), T.Val("6.1"));
  T.CheckFalse(T.Val(7.1), T.Val("7.1"));
  T.CheckTrue(T.Val(7.1), T.Val("8.1"));
}


TEST(BinopLessThanOrEqual) {
  FunctionTester T("(function(a,b) { return a <= b; })");

  T.CheckTrue(7, 8);
  T.CheckFalse(8, 7);
  T.CheckTrue(-8.1, -8);
  T.CheckFalse(-8, -8.1);
  T.CheckTrue(0.111, 0.111);

  T.CheckTrue(T.Val("7.1"), T.Val("7.1"));
  T.CheckFalse(T.Val(7.1), T.Val("6.1"));
  T.CheckTrue(T.Val(7.1), T.Val("7.1"));
  T.CheckTrue(T.Val(7.1), T.Val("8.1"));
}


TEST(BinopGreaterThan) {
  FunctionTester T("(function(a,b) { return a > b; })");

  T.CheckFalse(7, 8);
  T.CheckTrue(8, 7);
  T.CheckFalse(-8.1, -8);
  T.CheckTrue(-8, -8.1);
  T.CheckFalse(0.111, 0.111);

  T.CheckFalse(T.Val("7.1"), T.Val("7.1"));
  T.CheckTrue(T.Val(7.1), T.Val("6.1"));
  T.CheckFalse(T.Val(7.1), T.Val("7.1"));
  T.CheckFalse(T.Val(7.1), T.Val("8.1"));
}


TEST(BinopGreaterThanOrEqual) {
  FunctionTester T("(function(a,b) { return a >= b; })");

  T.CheckFalse(7, 8);
  T.CheckTrue(8, 7);
  T.CheckFalse(-8.1, -8);
  T.CheckTrue(-8, -8.1);
  T.CheckTrue(0.111, 0.111);

  T.CheckTrue(T.Val("7.1"), T.Val("7.1"));
  T.CheckTrue(T.Val(7.1), T.Val("6.1"));
  T.CheckTrue(T.Val(7.1), T.Val("7.1"));
  T.CheckFalse(T.Val(7.1), T.Val("8.1"));
}


TEST(BinopIn) {
  FunctionTester T("(function(a,b) { return a in b; })");

  T.CheckTrue(T.Val("x"), T.NewObject("({x:23})"));
  T.CheckFalse(T.Val("y"), T.NewObject("({x:42})"));
  T.CheckFalse(T.Val(123), T.NewObject("({x:65})"));
  T.CheckTrue(T.Val(1), T.NewObject("([1,2,3])"));
}


TEST(BinopInstanceOf) {
  FunctionTester T("(function(a,b) { return a instanceof b; })");

  T.CheckTrue(T.NewObject("(new Number(23))"), T.NewObject("Number"));
  T.CheckFalse(T.NewObject("(new Number(23))"), T.NewObject("String"));
  T.CheckFalse(T.NewObject("(new String('a'))"), T.NewObject("Number"));
  T.CheckTrue(T.NewObject("(new String('b'))"), T.NewObject("String"));
  T.CheckFalse(T.Val(1), T.NewObject("Number"));
  T.CheckFalse(T.Val("abc"), T.NewObject("String"));

  CompileRun("var bound = (function() {}).bind(undefined)");
  T.CheckTrue(T.NewObject("(new bound())"), T.NewObject("bound"));
  T.CheckTrue(T.NewObject("(new bound())"), T.NewObject("Object"));
  T.CheckFalse(T.NewObject("(new bound())"), T.NewObject("Number"));
}


TEST(UnopNot) {
  FunctionTester T("(function(a) { return !a; })");

  T.CheckCall(T.true_value(), T.false_value(), T.undefined());
  T.CheckCall(T.false_value(), T.true_value(), T.undefined());
  T.CheckCall(T.true_value(), T.Val(0.0), T.undefined());
  T.CheckCall(T.false_value(), T.Val(123), T.undefined());
  T.CheckCall(T.false_value(), T.Val("x"), T.undefined());
  T.CheckCall(T.true_value(), T.undefined(), T.undefined());
  T.CheckCall(T.true_value(), T.nan(), T.undefined());
}


TEST(UnopCountPost) {
  FunctionTester T("(function(a) { return a++; })");

  T.CheckCall(T.Val(0.0), T.Val(0.0), T.undefined());
  T.CheckCall(T.Val(2.3), T.Val(2.3), T.undefined());
  T.CheckCall(T.Val(123), T.Val(123), T.undefined());
  T.CheckCall(T.Val(7), T.Val("7"), T.undefined());
  T.CheckCall(T.nan(), T.Val("x"), T.undefined());
  T.CheckCall(T.nan(), T.undefined(), T.undefined());
  T.CheckCall(T.Val(1.0), T.true_value(), T.undefined());
  T.CheckCall(T.Val(0.0), T.false_value(), T.undefined());
  T.CheckCall(T.nan(), T.nan(), T.undefined());
}


TEST(UnopCountPre) {
  FunctionTester T("(function(a) { return ++a; })");

  T.CheckCall(T.Val(1.0), T.Val(0.0), T.undefined());
  T.CheckCall(T.Val(3.3), T.Val(2.3), T.undefined());
  T.CheckCall(T.Val(124), T.Val(123), T.undefined());
  T.CheckCall(T.Val(8), T.Val("7"), T.undefined());
  T.CheckCall(T.nan(), T.Val("x"), T.undefined());
  T.CheckCall(T.nan(), T.undefined(), T.undefined());
  T.CheckCall(T.Val(2.0), T.true_value(), T.undefined());
  T.CheckCall(T.Val(1.0), T.false_value(), T.undefined());
  T.CheckCall(T.nan(), T.nan(), T.undefined());
}


TEST(PropertyNamedLoad) {
  FunctionTester T("(function(a,b) { return a.x; })");

  T.CheckCall(T.Val(23), T.NewObject("({x:23})"), T.undefined());
  T.CheckCall(T.undefined(), T.NewObject("({y:23})"), T.undefined());
}


TEST(PropertyKeyedLoad) {
  FunctionTester T("(function(a,b) { return a[b]; })");

  T.CheckCall(T.Val(23), T.NewObject("({x:23})"), T.Val("x"));
  T.CheckCall(T.Val(42), T.NewObject("([23,42,65])"), T.Val(1));
  T.CheckCall(T.undefined(), T.NewObject("({x:23})"), T.Val("y"));
  T.CheckCall(T.undefined(), T.NewObject("([23,42,65])"), T.Val(4));
}


TEST(PropertyNamedStore) {
  FunctionTester T("(function(a) { a.x = 7; return a.x; })");

  T.CheckCall(T.Val(7), T.NewObject("({})"), T.undefined());
  T.CheckCall(T.Val(7), T.NewObject("({x:23})"), T.undefined());
}


TEST(PropertyKeyedStore) {
  FunctionTester T("(function(a,b) { a[b] = 7; return a.x; })");

  T.CheckCall(T.Val(7), T.NewObject("({})"), T.Val("x"));
  T.CheckCall(T.Val(7), T.NewObject("({x:23})"), T.Val("x"));
  T.CheckCall(T.Val(9), T.NewObject("({x:9})"), T.Val("y"));
}


TEST(PropertyNamedDelete) {
  FunctionTester T("(function(a) { return delete a.x; })");

  CompileRun("var o = Object.create({}, { x: { value:23 } });");
  T.CheckTrue(T.NewObject("({x:42})"), T.undefined());
  T.CheckTrue(T.NewObject("({})"), T.undefined());
  T.CheckFalse(T.NewObject("(o)"), T.undefined());
}


TEST(PropertyKeyedDelete) {
  FunctionTester T("(function(a, b) { return delete a[b]; })");

  CompileRun("function getX() { return 'x'; }");
  CompileRun("var o = Object.create({}, { x: { value:23 } });");
  T.CheckTrue(T.NewObject("({x:42})"), T.Val("x"));
  T.CheckFalse(T.NewObject("(o)"), T.Val("x"));
  T.CheckFalse(T.NewObject("(o)"), T.NewObject("({toString:getX})"));
}


TEST(GlobalLoad) {
  FunctionTester T("(function() { return g; })");

  T.CheckThrows(T.undefined(), T.undefined());
  CompileRun("var g = 23;");
  T.CheckCall(T.Val(23));
}


TEST(GlobalStoreStrict) {
  FunctionTester T("(function(a,b) { 'use strict'; g = a + b; return g; })");

  T.CheckThrows(T.Val(22), T.Val(11));
  CompileRun("var g = 'a global variable';");
  T.CheckCall(T.Val(33), T.Val(22), T.Val(11));
}


TEST(ContextLoad) {
  FunctionTester T("(function(a,b) { (function(){a}); return a + b; })");

  T.CheckCall(T.Val(65), T.Val(23), T.Val(42));
  T.CheckCall(T.Val("ab"), T.Val("a"), T.Val("b"));
}


TEST(ContextStore) {
  FunctionTester T("(function(a,b) { (function(){x}); var x = a; return x; })");

  T.CheckCall(T.Val(23), T.Val(23), T.undefined());
  T.CheckCall(T.Val("a"), T.Val("a"), T.undefined());
}


TEST(LookupLoad) {
  FunctionTester T("(function(a,b) { with(a) { return x + b; } })");

  T.CheckCall(T.Val(24), T.NewObject("({x:23})"), T.Val(1));
  T.CheckCall(T.Val(32), T.NewObject("({x:23, b:9})"), T.Val(2));
  T.CheckCall(T.Val(45), T.NewObject("({__proto__:{x:42}})"), T.Val(3));
  T.CheckCall(T.Val(69), T.NewObject("({get x() { return 65; }})"), T.Val(4));
}


TEST(LookupStore) {
  FunctionTester T("(function(a,b) { var x; with(a) { x = b; } return x; })");

  T.CheckCall(T.undefined(), T.NewObject("({x:23})"), T.Val(1));
  T.CheckCall(T.Val(2), T.NewObject("({y:23})"), T.Val(2));
  T.CheckCall(T.Val(23), T.NewObject("({b:23})"), T.Val(3));
  T.CheckCall(T.undefined(), T.NewObject("({__proto__:{x:42}})"), T.Val(4));
}


TEST(BlockLoadStore) {
  FunctionTester T("(function(a) { 'use strict'; { let x = a+a; return x; }})");

  T.CheckCall(T.Val(46), T.Val(23));
  T.CheckCall(T.Val("aa"), T.Val("a"));
}


TEST(BlockLoadStoreNested) {
  const char* src =
      "(function(a,b) {"
      "'use strict';"
      "{ let x = a, y = a;"
      "  { let y = b;"
      "    return x + y;"
      "  }"
      "}})";
  FunctionTester T(src);

  T.CheckCall(T.Val(65), T.Val(23), T.Val(42));
  T.CheckCall(T.Val("ab"), T.Val("a"), T.Val("b"));
}


TEST(ObjectLiteralComputed) {
  FunctionTester T("(function(a,b) { o = { x:a+b }; return o.x; })");

  T.CheckCall(T.Val(65), T.Val(23), T.Val(42));
  T.CheckCall(T.Val("ab"), T.Val("a"), T.Val("b"));
}


TEST(ObjectLiteralNonString) {
  FunctionTester T("(function(a,b) { o = { 7:a+b }; return o[7]; })");

  T.CheckCall(T.Val(65), T.Val(23), T.Val(42));
  T.CheckCall(T.Val("ab"), T.Val("a"), T.Val("b"));
}


TEST(ObjectLiteralPrototype) {
  FunctionTester T("(function(a) { o = { __proto__:a }; return o.x; })");

  T.CheckCall(T.Val(23), T.NewObject("({x:23})"), T.undefined());
  T.CheckCall(T.undefined(), T.NewObject("({y:42})"), T.undefined());
}


TEST(ObjectLiteralGetter) {
  FunctionTester T("(function(a) { o = { get x() {return a} }; return o.x; })");

  T.CheckCall(T.Val(23), T.Val(23), T.undefined());
  T.CheckCall(T.Val("x"), T.Val("x"), T.undefined());
}


TEST(ArrayLiteral) {
  FunctionTester T("(function(a,b) { o = [1, a + b, 3]; return o[1]; })");

  T.CheckCall(T.Val(65), T.Val(23), T.Val(42));
  T.CheckCall(T.Val("ab"), T.Val("a"), T.Val("b"));
}


TEST(RegExpLiteral) {
  FunctionTester T("(function(a) { o = /b/; return o.test(a); })");

  T.CheckTrue(T.Val("abc"));
  T.CheckFalse(T.Val("xyz"));
}


TEST(ClassLiteral) {
  const char* src =
      "(function(a,b) {"
      "  class C {"
      "    x() { return a; }"
      "    static y() { return b; }"
      "    get z() { return 0; }"
      "    constructor() {}"
      "  }"
      "  return new C().x() + C.y();"
      "})";
  FunctionTester T(src);

  T.CheckCall(T.Val(65), T.Val(23), T.Val(42));
  T.CheckCall(T.Val("ab"), T.Val("a"), T.Val("b"));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
