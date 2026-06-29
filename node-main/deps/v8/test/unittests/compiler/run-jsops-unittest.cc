// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/objects-inl.h"
#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

using RunJSOpsTest = TestWithContext;

TEST_F(RunJSOpsTest, BinopAdd) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a + b; })");

  T.CheckCall(3, 1, 2);
  T.CheckCall(-11, -2, -9);
  T.CheckCall(-11, -1.5, -9.5);
  T.CheckCall(T.NewString("AB"), T.NewString("A"), T.NewString("B"));
  T.CheckCall(T.NewString("A11"), T.NewString("A"), T.NewNumber(11));
  T.CheckCall(T.NewString("12B"), T.NewNumber(12), T.NewString("B"));
  T.CheckCall(T.NewString("38"), T.NewString("3"), T.NewString("8"));
  T.CheckCall(T.NewString("31"), T.NewString("3"), T.NewObject("([1])"));
  T.CheckCall(T.NewString("3[object Object]"), T.NewString("3"),
              T.NewObject("({})"));
}

TEST_F(RunJSOpsTest, BinopSubtract) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a - b; })");

  T.CheckCall(3, 4, 1);
  T.CheckCall(3.0, 4.5, 1.5);
  T.CheckCall(T.NewNumber(-9), T.NewString("0"), T.NewNumber(9));
  T.CheckCall(T.NewNumber(-9), T.NewNumber(0.0), T.NewString("9"));
  T.CheckCall(T.NewNumber(1), T.NewString("3"), T.NewString("2"));
  T.CheckCall(T.nan(), T.NewString("3"), T.NewString("B"));
  T.CheckCall(T.NewNumber(2), T.NewString("3"), T.NewObject("([1])"));
  T.CheckCall(T.nan(), T.NewString("3"), T.NewObject("({})"));
}

TEST_F(RunJSOpsTest, BinopMultiply) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a * b; })");

  T.CheckCall(6, 3, 2);
  T.CheckCall(4.5, 2.0, 2.25);
  T.CheckCall(T.NewNumber(6), T.NewString("3"), T.NewNumber(2));
  T.CheckCall(T.NewNumber(4.5), T.NewNumber(2.0), T.NewString("2.25"));
  T.CheckCall(T.NewNumber(6), T.NewString("3"), T.NewString("2"));
  T.CheckCall(T.nan(), T.NewString("3"), T.NewString("B"));
  T.CheckCall(T.NewNumber(3), T.NewString("3"), T.NewObject("([1])"));
  T.CheckCall(T.nan(), T.NewString("3"), T.NewObject("({})"));
}

TEST_F(RunJSOpsTest, BinopDivide) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a / b; })");

  T.CheckCall(2, 8, 4);
  T.CheckCall(2.1, 8.4, 4);
  T.CheckCall(V8_INFINITY, 8, 0);
  T.CheckCall(-V8_INFINITY, -8, 0);
  T.CheckCall(T.infinity(), T.NewNumber(8), T.NewString("0"));
  T.CheckCall(T.minus_infinity(), T.NewString("-8"), T.NewNumber(0.0));
  T.CheckCall(T.NewNumber(1.5), T.NewString("3"), T.NewString("2"));
  T.CheckCall(T.nan(), T.NewString("3"), T.NewString("B"));
  T.CheckCall(T.NewNumber(1.5), T.NewString("3"), T.NewObject("([2])"));
  T.CheckCall(T.nan(), T.NewString("3"), T.NewObject("({})"));
}

TEST_F(RunJSOpsTest, BinopModulus) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a % b; })");

  T.CheckCall(3, 8, 5);
  T.CheckCall(T.NewNumber(3), T.NewString("8"), T.NewNumber(5));
  T.CheckCall(T.NewNumber(3), T.NewNumber(8), T.NewString("5"));
  T.CheckCall(T.NewNumber(1), T.NewString("3"), T.NewString("2"));
  T.CheckCall(T.nan(), T.NewString("3"), T.NewString("B"));
  T.CheckCall(T.NewNumber(1), T.NewString("3"), T.NewObject("([2])"));
  T.CheckCall(T.nan(), T.NewString("3"), T.NewObject("({})"));
}

TEST_F(RunJSOpsTest, BinopShiftLeft) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a << b; })");

  T.CheckCall(4, 2, 1);
  T.CheckCall(T.NewNumber(4), T.NewString("2"), T.NewNumber(1));
  T.CheckCall(T.NewNumber(4), T.NewNumber(2), T.NewString("1"));
}

TEST_F(RunJSOpsTest, BinopShiftRight) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a >> b; })");

  T.CheckCall(4, 8, 1);
  T.CheckCall(-4, -8, 1);
  T.CheckCall(T.NewNumber(4), T.NewString("8"), T.NewNumber(1));
  T.CheckCall(T.NewNumber(4), T.NewNumber(8), T.NewString("1"));
}

TEST_F(RunJSOpsTest, BinopShiftRightLogical) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a >>> b; })");

  T.CheckCall(4, 8, 1);
  T.CheckCall(0x7FFFFFFC, -8, 1);
  T.CheckCall(T.NewNumber(4), T.NewString("8"), T.NewNumber(1));
  T.CheckCall(T.NewNumber(4), T.NewNumber(8), T.NewString("1"));
}

TEST_F(RunJSOpsTest, BinopAnd) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a & b; })");

  T.CheckCall(7, 7, 15);
  T.CheckCall(7, 15, 7);
  T.CheckCall(T.NewNumber(7), T.NewString("15"), T.NewNumber(7));
  T.CheckCall(T.NewNumber(7), T.NewNumber(15), T.NewString("7"));
}

TEST_F(RunJSOpsTest, BinopOr) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a | b; })");

  T.CheckCall(6, 4, 2);
  T.CheckCall(6, 2, 4);
  T.CheckCall(T.NewNumber(6), T.NewString("2"), T.NewNumber(4));
  T.CheckCall(T.NewNumber(6), T.NewNumber(2), T.NewString("4"));
}

TEST_F(RunJSOpsTest, BinopXor) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a ^ b; })");

  T.CheckCall(7, 15, 8);
  T.CheckCall(7, 8, 15);
  T.CheckCall(T.NewNumber(7), T.NewString("8"), T.NewNumber(15));
  T.CheckCall(T.NewNumber(7), T.NewNumber(8), T.NewString("15"));
}

TEST_F(RunJSOpsTest, BinopStrictEqual) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a === b; })");

  T.CheckTrue(7, 7);
  T.CheckFalse(7, 8);
  T.CheckTrue(7.1, 7.1);
  T.CheckFalse(7.1, 8.1);

  T.CheckTrue(T.NewString("7.1"), T.NewString("7.1"));
  T.CheckFalse(T.NewNumber(7.1), T.NewString("7.1"));
  T.CheckFalse(T.NewNumber(7), T.undefined());
  T.CheckFalse(T.undefined(), T.NewNumber(7));

  TryRunJS("var o = { desc : 'I am a singleton' }");
  T.CheckFalse(T.NewObject("([1])"), T.NewObject("([1])"));
  T.CheckFalse(T.NewObject("({})"), T.NewObject("({})"));
  T.CheckTrue(T.NewObject("(o)"), T.NewObject("(o)"));
}

TEST_F(RunJSOpsTest, BinopEqual) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a == b; })");

  T.CheckTrue(7, 7);
  T.CheckFalse(7, 8);
  T.CheckTrue(7.1, 7.1);
  T.CheckFalse(7.1, 8.1);

  T.CheckTrue(T.NewString("7.1"), T.NewString("7.1"));
  T.CheckTrue(T.NewNumber(7.1), T.NewString("7.1"));

  TryRunJS("var o = { desc : 'I am a singleton' }");
  T.CheckFalse(T.NewObject("([1])"), T.NewObject("([1])"));
  T.CheckFalse(T.NewObject("({})"), T.NewObject("({})"));
  T.CheckTrue(T.NewObject("(o)"), T.NewObject("(o)"));
}

TEST_F(RunJSOpsTest, BinopNotEqual) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a != b; })");

  T.CheckFalse(7, 7);
  T.CheckTrue(7, 8);
  T.CheckFalse(7.1, 7.1);
  T.CheckTrue(7.1, 8.1);

  T.CheckFalse(T.NewString("7.1"), T.NewString("7.1"));
  T.CheckFalse(T.NewNumber(7.1), T.NewString("7.1"));

  TryRunJS("var o = { desc : 'I am a singleton' }");
  T.CheckTrue(T.NewObject("([1])"), T.NewObject("([1])"));
  T.CheckTrue(T.NewObject("({})"), T.NewObject("({})"));
  T.CheckFalse(T.NewObject("(o)"), T.NewObject("(o)"));
}

TEST_F(RunJSOpsTest, BinopLessThan) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a < b; })");

  T.CheckTrue(7, 8);
  T.CheckFalse(8, 7);
  T.CheckTrue(-8.1, -8);
  T.CheckFalse(-8, -8.1);
  T.CheckFalse(0.111, 0.111);

  T.CheckFalse(T.NewString("7.1"), T.NewString("7.1"));
  T.CheckFalse(T.NewNumber(7.1), T.NewString("6.1"));
  T.CheckFalse(T.NewNumber(7.1), T.NewString("7.1"));
  T.CheckTrue(T.NewNumber(7.1), T.NewString("8.1"));
}

TEST_F(RunJSOpsTest, BinopLessThanOrEqual) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a <= b; })");

  T.CheckTrue(7, 8);
  T.CheckFalse(8, 7);
  T.CheckTrue(-8.1, -8);
  T.CheckFalse(-8, -8.1);
  T.CheckTrue(0.111, 0.111);

  T.CheckTrue(T.NewString("7.1"), T.NewString("7.1"));
  T.CheckFalse(T.NewNumber(7.1), T.NewString("6.1"));
  T.CheckTrue(T.NewNumber(7.1), T.NewString("7.1"));
  T.CheckTrue(T.NewNumber(7.1), T.NewString("8.1"));
}

TEST_F(RunJSOpsTest, BinopGreaterThan) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a > b; })");

  T.CheckFalse(7, 8);
  T.CheckTrue(8, 7);
  T.CheckFalse(-8.1, -8);
  T.CheckTrue(-8, -8.1);
  T.CheckFalse(0.111, 0.111);

  T.CheckFalse(T.NewString("7.1"), T.NewString("7.1"));
  T.CheckTrue(T.NewNumber(7.1), T.NewString("6.1"));
  T.CheckFalse(T.NewNumber(7.1), T.NewString("7.1"));
  T.CheckFalse(T.NewNumber(7.1), T.NewString("8.1"));
}

TEST_F(RunJSOpsTest, BinopGreaterThanOrEqual) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a >= b; })");

  T.CheckFalse(7, 8);
  T.CheckTrue(8, 7);
  T.CheckFalse(-8.1, -8);
  T.CheckTrue(-8, -8.1);
  T.CheckTrue(0.111, 0.111);

  T.CheckTrue(T.NewString("7.1"), T.NewString("7.1"));
  T.CheckTrue(T.NewNumber(7.1), T.NewString("6.1"));
  T.CheckTrue(T.NewNumber(7.1), T.NewString("7.1"));
  T.CheckFalse(T.NewNumber(7.1), T.NewString("8.1"));
}

TEST_F(RunJSOpsTest, BinopIn) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a in b; })");

  T.CheckTrue(T.NewString("x"), T.NewObject("({x:23})"));
  T.CheckFalse(T.NewString("y"), T.NewObject("({x:42})"));
  T.CheckFalse(T.NewNumber(123), T.NewObject("({x:65})"));
  T.CheckTrue(T.NewNumber(1), T.NewObject("([1,2,3])"));
}

TEST_F(RunJSOpsTest, BinopInstanceOf) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a instanceof b; })");

  T.CheckTrue(T.NewObject("(new Number(23))"), T.NewObject("Number"));
  T.CheckFalse(T.NewObject("(new Number(23))"), T.NewObject("String"));
  T.CheckFalse(T.NewObject("(new String('a'))"), T.NewObject("Number"));
  T.CheckTrue(T.NewObject("(new String('b'))"), T.NewObject("String"));
  T.CheckFalse(T.NewNumber(1), T.NewObject("Number"));
  T.CheckFalse(T.NewString("abc"), T.NewObject("String"));

  TryRunJS("var bound = (function() {}).bind(undefined)");
  T.CheckTrue(T.NewObject("(new bound())"), T.NewObject("bound"));
  T.CheckTrue(T.NewObject("(new bound())"), T.NewObject("Object"));
  T.CheckFalse(T.NewObject("(new bound())"), T.NewObject("Number"));
}

TEST_F(RunJSOpsTest, UnopNot) {
  FunctionTester T(i_isolate(), "(function(a) { return !a; })");

  T.CheckCall(T.true_value(), T.false_value(), T.undefined());
  T.CheckCall(T.false_value(), T.true_value(), T.undefined());
  T.CheckCall(T.true_value(), T.NewNumber(0.0), T.undefined());
  T.CheckCall(T.false_value(), T.NewNumber(123), T.undefined());
  T.CheckCall(T.false_value(), T.NewString("x"), T.undefined());
  T.CheckCall(T.true_value(), T.undefined(), T.undefined());
  T.CheckCall(T.true_value(), T.nan(), T.undefined());
}

TEST_F(RunJSOpsTest, UnopCountPost) {
  FunctionTester T(i_isolate(), "(function(a) { return a++; })");

  T.CheckCall(T.NewNumber(0.0), T.NewNumber(0.0), T.undefined());
  T.CheckCall(T.NewNumber(2.3), T.NewNumber(2.3), T.undefined());
  T.CheckCall(T.NewNumber(123), T.NewNumber(123), T.undefined());
  T.CheckCall(T.NewNumber(7), T.NewString("7"), T.undefined());
  T.CheckCall(T.nan(), T.NewString("x"), T.undefined());
  T.CheckCall(T.nan(), T.undefined(), T.undefined());
  T.CheckCall(T.NewNumber(1.0), T.true_value(), T.undefined());
  T.CheckCall(T.NewNumber(0.0), T.false_value(), T.undefined());
  T.CheckCall(T.nan(), T.nan(), T.undefined());
}

TEST_F(RunJSOpsTest, UnopCountPre) {
  FunctionTester T(i_isolate(), "(function(a) { return ++a; })");

  T.CheckCall(T.NewNumber(1.0), T.NewNumber(0.0), T.undefined());
  T.CheckCall(T.NewNumber(3.3), T.NewNumber(2.3), T.undefined());
  T.CheckCall(T.NewNumber(124), T.NewNumber(123), T.undefined());
  T.CheckCall(T.NewNumber(8), T.NewString("7"), T.undefined());
  T.CheckCall(T.nan(), T.NewString("x"), T.undefined());
  T.CheckCall(T.nan(), T.undefined(), T.undefined());
  T.CheckCall(T.NewNumber(2.0), T.true_value(), T.undefined());
  T.CheckCall(T.NewNumber(1.0), T.false_value(), T.undefined());
  T.CheckCall(T.nan(), T.nan(), T.undefined());
}

TEST_F(RunJSOpsTest, PropertyNamedLoad) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a.x; })");

  T.CheckCall(T.NewNumber(23), T.NewObject("({x:23})"), T.undefined());
  T.CheckCall(T.undefined(), T.NewObject("({y:23})"), T.undefined());
}

TEST_F(RunJSOpsTest, PropertyKeyedLoad) {
  FunctionTester T(i_isolate(), "(function(a,b) { return a[b]; })");

  T.CheckCall(T.NewNumber(23), T.NewObject("({x:23})"), T.NewString("x"));
  T.CheckCall(T.NewNumber(42), T.NewObject("([23,42,65])"), T.NewNumber(1));
  T.CheckCall(T.undefined(), T.NewObject("({x:23})"), T.NewString("y"));
  T.CheckCall(T.undefined(), T.NewObject("([23,42,65])"), T.NewNumber(4));
}

TEST_F(RunJSOpsTest, PropertyNamedStore) {
  FunctionTester T(i_isolate(), "(function(a) { a.x = 7; return a.x; })");

  T.CheckCall(T.NewNumber(7), T.NewObject("({})"), T.undefined());
  T.CheckCall(T.NewNumber(7), T.NewObject("({x:23})"), T.undefined());
}

TEST_F(RunJSOpsTest, PropertyKeyedStore) {
  FunctionTester T(i_isolate(), "(function(a,b) { a[b] = 7; return a.x; })");

  T.CheckCall(T.NewNumber(7), T.NewObject("({})"), T.NewString("x"));
  T.CheckCall(T.NewNumber(7), T.NewObject("({x:23})"), T.NewString("x"));
  T.CheckCall(T.NewNumber(9), T.NewObject("({x:9})"), T.NewString("y"));
}

TEST_F(RunJSOpsTest, PropertyNamedDelete) {
  FunctionTester T(i_isolate(), "(function(a) { return delete a.x; })");

  TryRunJS("var o = Object.create({}, { x: { value:23 } });");
  T.CheckTrue(T.NewObject("({x:42})"), T.undefined());
  T.CheckTrue(T.NewObject("({})"), T.undefined());
  T.CheckFalse(T.NewObject("(o)"), T.undefined());
}

TEST_F(RunJSOpsTest, PropertyKeyedDelete) {
  FunctionTester T(i_isolate(), "(function(a, b) { return delete a[b]; })");

  TryRunJS("function getX() { return 'x'; }");
  TryRunJS("var o = Object.create({}, { x: { value:23 } });");
  T.CheckTrue(T.NewObject("({x:42})"), T.NewString("x"));
  T.CheckFalse(T.NewObject("(o)"), T.NewString("x"));
  T.CheckFalse(T.NewObject("(o)"), T.NewObject("({toString:getX})"));
}

TEST_F(RunJSOpsTest, GlobalLoad) {
  FunctionTester T(i_isolate(), "(function() { return g; })");

  T.CheckThrows(T.undefined(), T.undefined());
  TryRunJS("var g = 23;");
  T.CheckCall(T.NewNumber(23));
}

TEST_F(RunJSOpsTest, GlobalStoreStrict) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { 'use strict'; g = a + b; return g; })");

  T.CheckThrows(T.NewNumber(22), T.NewNumber(11));
  TryRunJS("var g = 'a global variable';");
  T.CheckCall(T.NewNumber(33), T.NewNumber(22), T.NewNumber(11));
}

TEST_F(RunJSOpsTest, ContextLoad) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { (function(){a}); return a + b; })");

  T.CheckCall(T.NewNumber(65), T.NewNumber(23), T.NewNumber(42));
  T.CheckCall(T.NewString("ab"), T.NewString("a"), T.NewString("b"));
}

TEST_F(RunJSOpsTest, ContextStore) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { (function(){x}); var x = a; return x; })");

  T.CheckCall(T.NewNumber(23), T.NewNumber(23), T.undefined());
  T.CheckCall(T.NewString("a"), T.NewString("a"), T.undefined());
}

TEST_F(RunJSOpsTest, LookupLoad) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { with(a) { return x + b; } })");

  T.CheckCall(T.NewNumber(24), T.NewObject("({x:23})"), T.NewNumber(1));
  T.CheckCall(T.NewNumber(32), T.NewObject("({x:23, b:9})"), T.NewNumber(2));
  T.CheckCall(T.NewNumber(45), T.NewObject("({__proto__:{x:42}})"),
              T.NewNumber(3));
  T.CheckCall(T.NewNumber(69), T.NewObject("({get x() { return 65; }})"),
              T.NewNumber(4));
}

TEST_F(RunJSOpsTest, LookupStore) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { var x; with(a) { x = b; } return x; })");

  T.CheckCall(T.undefined(), T.NewObject("({x:23})"), T.NewNumber(1));
  T.CheckCall(T.NewNumber(2), T.NewObject("({y:23})"), T.NewNumber(2));
  T.CheckCall(T.NewNumber(23), T.NewObject("({b:23})"), T.NewNumber(3));
  T.CheckCall(T.undefined(), T.NewObject("({__proto__:{x:42}})"),
              T.NewNumber(4));
}

TEST_F(RunJSOpsTest, BlockLoadStore) {
  FunctionTester T(i_isolate(),
                   "(function(a) { 'use strict'; { let x = a+a; return x; }})");

  T.CheckCall(T.NewNumber(46), T.NewNumber(23));
  T.CheckCall(T.NewString("aa"), T.NewString("a"));
}

TEST_F(RunJSOpsTest, BlockLoadStoreNested) {
  const char* src =
      "(function(a,b) {"
      "'use strict';"
      "{ let x = a, y = a;"
      "  { let y = b;"
      "    return x + y;"
      "  }"
      "}})";
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewNumber(65), T.NewNumber(23), T.NewNumber(42));
  T.CheckCall(T.NewString("ab"), T.NewString("a"), T.NewString("b"));
}

TEST_F(RunJSOpsTest, ObjectLiteralComputed) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { o = { x:a+b }; return o.x; })");

  T.CheckCall(T.NewNumber(65), T.NewNumber(23), T.NewNumber(42));
  T.CheckCall(T.NewString("ab"), T.NewString("a"), T.NewString("b"));
}

TEST_F(RunJSOpsTest, ObjectLiteralNonString) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { o = { 7:a+b }; return o[7]; })");

  T.CheckCall(T.NewNumber(65), T.NewNumber(23), T.NewNumber(42));
  T.CheckCall(T.NewString("ab"), T.NewString("a"), T.NewString("b"));
}

TEST_F(RunJSOpsTest, ObjectLiteralPrototype) {
  FunctionTester T(i_isolate(),
                   "(function(a) { o = { __proto__:a }; return o.x; })");

  T.CheckCall(T.NewNumber(23), T.NewObject("({x:23})"), T.undefined());
  T.CheckCall(T.undefined(), T.NewObject("({y:42})"), T.undefined());
}

TEST_F(RunJSOpsTest, ObjectLiteralGetter) {
  FunctionTester T(i_isolate(),
                   "(function(a) { o = { get x() {return a} }; return o.x; })");

  T.CheckCall(T.NewNumber(23), T.NewNumber(23), T.undefined());
  T.CheckCall(T.NewString("x"), T.NewString("x"), T.undefined());
}

TEST_F(RunJSOpsTest, ArrayLiteral) {
  FunctionTester T(i_isolate(),
                   "(function(a,b) { o = [1, a + b, 3]; return o[1]; })");

  T.CheckCall(T.NewNumber(65), T.NewNumber(23), T.NewNumber(42));
  T.CheckCall(T.NewString("ab"), T.NewString("a"), T.NewString("b"));
}

TEST_F(RunJSOpsTest, RegExpLiteral) {
  FunctionTester T(i_isolate(), "(function(a) { o = /b/; return o.test(a); })");

  T.CheckTrue(T.NewString("abc"));
  T.CheckFalse(T.NewString("xyz"));
}

TEST_F(RunJSOpsTest, ClassLiteral) {
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
  FunctionTester T(i_isolate(), src);

  T.CheckCall(T.NewNumber(65), T.NewNumber(23), T.NewNumber(42));
  T.CheckCall(T.NewString("ab"), T.NewString("a"), T.NewString("b"));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
