// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function TestBasic() {
  assertEquals(-(8 ** 2), -64);
  assertEquals(+(8 ** 2), 64);
  assertEquals(~(8 ** 2), -65);
  assertEquals(!(8 ** 2), false);

  assertEquals(2 ** -2, 0.25);

  var o = { p: 1 };
  assertEquals(2 ** delete o.p, 2);

  assertEquals(2 ** void 1, NaN);

  assertEquals(2 ** typeof 1, NaN);

  var s = "2";
  var n = 2;

  assertEquals(2 ** "2", 4);
  assertEquals(2 ** +"2", 4);
  assertEquals(2 ** +s, 4);
  assertEquals(2 ** s, 4);
  assertEquals(2 ** 2, 4);
  assertEquals(2 ** +2, 4);
  assertEquals(2 ** +n, 4);
  assertEquals(2 ** n, 4);

  assertEquals(2 ** -"2", 0.25);
  assertEquals(2 ** -s, 0.25);
  assertEquals(2 ** -2, 0.25);
  assertEquals(2 ** -n, 0.25);

  assertEquals(2 ** ~"2", 0.125);
  assertEquals(2 ** ~s, 0.125);
  assertEquals(2 ** ~2, 0.125);
  assertEquals(2 ** ~n, 0.125);

  assertEquals(2 ** !"2", 1);
  assertEquals(2 ** !s, 1);
  assertEquals(2 ** !2, 1);
  assertEquals(2 ** !n, 1);

 var exponent = 2;
 assertEquals(2 ** 3, 8);
 assertEquals(3 * 2 ** 3, 24);
 assertEquals(2 ** ++exponent, 8);
 assertEquals(2 ** -1 * 2, 1);
 assertEquals(2 ** 2 * 4, 16);
 assertEquals(2 ** 2 / 2, 2);
 assertEquals(2 ** (3 ** 2), 512);
 assertEquals(2 ** 3 ** 2, 512);
 assertEquals(2 * 3 ** 2, 18);
 assertEquals(16 / 2 ** 2, 4);
}
TestBasic();


function TestAssignment() {
  var base = -5;
  assertEquals(base **= 3, -125);
  assertEquals(base, -125);
}
TestAssignment();


function TestPrecedence() {
  var base = 4;
  assertEquals(--base ** 2, 9);  // 3 ** 2
  assertEquals(++base ** 2, 16); // 4 ** 2
  assertEquals(base++ ** 2, 16); // 4 ** 2
  assertEquals(base-- ** 2, 25); // 5 ** 2

  assertEquals(4, base);
  assertEquals(--base ** --base ** 2,
               Math.pow(3, Math.pow(2, 2)));

  assertEquals(2, base);
  assertEquals(++base ** ++base ** 2,
               Math.pow(3, Math.pow(4, 2)));

  base = 4;
  assertEquals(base-- ** base-- ** 2,
               Math.pow(4, Math.pow(3, 2)));

  assertEquals(2, base);
  assertEquals(base++ ** base++ ** 2,
               Math.pow(2, Math.pow(3, 2)));
}
TestPrecedence();


function TestInvariants() {
  assertEquals(NaN, 2 ** NaN);
  assertEquals(NaN, (+0) ** NaN);
  assertEquals(NaN, (-0) ** NaN);
  assertEquals(NaN, Infinity ** NaN);
  assertEquals(NaN, (-Infinity) ** NaN);

  assertEquals(1, NaN ** +0);
  assertEquals(1, NaN ** -0);

  assertEquals(NaN, NaN ** NaN);
  assertEquals(NaN, NaN ** 2.2);
  assertEquals(NaN, NaN ** 1);
  assertEquals(NaN, NaN ** -1);
  assertEquals(NaN, NaN ** -2.2);
  assertEquals(NaN, NaN ** Infinity);
  assertEquals(NaN, NaN ** -Infinity);

  assertEquals(Infinity, 1.1 ** Infinity);
  assertEquals(Infinity, (-1.1) ** Infinity);
  assertEquals(Infinity, 2 ** Infinity);
  assertEquals(Infinity, (-2) ** Infinity);

  // Because +0 == -0, we need to compare 1/{+,-}0 to {+,-}Infinity
  assertEquals(+Infinity, 1/1.1 ** -Infinity);
  assertEquals(+Infinity, 1/(-1.1) ** -Infinity);
  assertEquals(+Infinity, 1/2 ** -Infinity);
  assertEquals(+Infinity, 1/(-2) ** -Infinity);

  assertEquals(NaN, 1 ** Infinity);
  assertEquals(NaN, 1 ** -Infinity);
  assertEquals(NaN, (-1) ** Infinity);
  assertEquals(NaN, (-1) ** -Infinity);

  assertEquals(+0, 0.1 ** Infinity);
  assertEquals(+0, (-0.1) ** Infinity);
  assertEquals(+0, 0.999 ** Infinity);
  assertEquals(+0, (-0.999) ** Infinity);

  assertEquals(Infinity, 0.1 ** -Infinity);
  assertEquals(Infinity, (-0.1) ** -Infinity);
  assertEquals(Infinity, 0.999 ** -Infinity);
  assertEquals(Infinity, (-0.999) ** -Infinity);

  assertEquals(Infinity, Infinity ** 0.1);
  assertEquals(Infinity, Infinity ** 2);

  assertEquals(+Infinity, 1/Infinity ** -0.1);
  assertEquals(+Infinity, 1/Infinity ** -2);

  assertEquals(-Infinity, (-Infinity) ** 3);
  assertEquals(-Infinity, (-Infinity) ** 13);

  assertEquals(Infinity, (-Infinity) ** 3.1);
  assertEquals(Infinity, (-Infinity) ** 2);

  assertEquals(-Infinity, 1/(-Infinity) ** -3);
  assertEquals(-Infinity, 1/(-Infinity) ** -13);

  assertEquals(+Infinity, 1/(-Infinity) ** -3.1);
  assertEquals(+Infinity, 1/(-Infinity) ** -2);

  assertEquals(+Infinity, 1/(+0) ** 1.1);
  assertEquals(+Infinity, 1/(+0) ** 2);

  assertEquals(Infinity, (+0) ** -1.1);
  assertEquals(Infinity, (+0) ** -2);

  assertEquals(-Infinity, 1/(-0) ** 3);
  assertEquals(-Infinity, 1/(-0) ** 13);

  assertEquals(+Infinity, 1/(-0) ** 3.1);
  assertEquals(+Infinity, 1/(-0) ** 2);

  assertEquals(-Infinity, (-0) ** -3);
  assertEquals(-Infinity, (-0) ** -13);

  assertEquals(Infinity, (-0) ** -3.1);
  assertEquals(Infinity, (-0) ** -2);

  assertEquals(NaN, (-0.00001) ** 1.1);
  assertEquals(NaN, (-0.00001) ** -1.1);
  assertEquals(NaN, (-1.1) ** 1.1);
  assertEquals(NaN, (-1.1) ** -1.1);
  assertEquals(NaN, (-2) ** 1.1);
  assertEquals(NaN, (-2) ** -1.1);
  assertEquals(NaN, (-1000) ** 1.1);
  assertEquals(NaN, (-1000) ** -1.1);

  assertEquals(+Infinity, 1/(-0) ** 0.5);
  assertEquals(+Infinity, 1/(-0) ** 0.6);
  assertEquals(-Infinity, 1/(-0) ** 1);
  assertEquals(-Infinity, 1/(-0) ** 10000000001);

  assertEquals(+Infinity, (-0) ** -0.5);
  assertEquals(+Infinity, (-0) ** -0.6);
  assertEquals(-Infinity, (-0) ** -1);
  assertEquals(-Infinity, (-0) ** -10000000001);

  assertEquals(4, 16 ** 0.5);
  assertEquals(NaN, (-16) ** 0.5);
  assertEquals(0.25, 16 ** -0.5);
  assertEquals(NaN, (-16) ** -0.5);
}
TestInvariants();


function TestOperationOrder() {
  var log = [];
  var handler = {
    get(t, n) {
      var result = Reflect.get(t, n);
      var str = typeof result === "object" ? "[object Object]" : String(result);
      log.push(`[[Get]](${String(n)}) -> ${str}`);
      return result;
    },
    set(t, n, v) {
      var result = Reflect.set(t, n, v);
      log.push(`[[Set]](${String(n)}, ${String(v)}) -> ${String(result)}`);
      return result;
    },
    has() { assertUnreachable("trap 'has' invoked"); },
    deleteProperty() { assertUnreachable("trap 'deleteProperty' invoked"); },
    ownKeys() { assertUnreachable("trap 'ownKeys' invoked"); },
    apply() { assertUnreachable("trap 'apply' invoked"); },
    construct() { assertUnreachable("trap 'construct' invoked"); },
    getPrototypeOf() { assertUnreachable("trap 'getPrototypeOf' invoked"); },
    setPrototypeOf() { assertUnreachable("trap 'setPrototypeOf' invoked"); },
    isExtensible() { assertUnreachable("trap 'isExtensible' invoked"); },
    preventExtensions() {
      assertUnreachable("trap 'preventExtensions' invoked"); },
    getOwnPropertyDescriptor() {
      assertUnreachable("trap 'getOwnPropertyDescriptor' invoked"); },
    defineProperty() { assertUnreachable("trap 'defineProperty' invoked"); },
  };
  var P = new Proxy({ x: 2 }, handler);

  assertEquals(256, P.x **= "8");
  assertEquals([
    "[[Get]](x) -> 2",
    "[[Set]](x, 256) -> true"
  ], log);

  log = [];
  var O = new Proxy({ p: P }, handler);
  assertEquals(65536, O.p.x **= 2 );
  assertEquals([
    "[[Get]](p) -> [object Object]",
    "[[Get]](x) -> 256",
    "[[Set]](x, 65536) -> true"
  ], log);
}
TestOperationOrder();


function TestOverrideMathPow() {
  var MathPow = MathPow;
  Math.pow = function(a, b) {
    assertUnreachable(`Math.pow(${String(a)}, ${String(b)}) invoked`);
  }

  TestBasic();
  TestAssignment();
  TestInvariants();
  TestOperationOrder();

  Math.pow = MathPow;
}
TestOverrideMathPow();

function TestBadAssignmentLHS() {
  assertThrows("if (false) { 17 **= 10; }", SyntaxError);
  assertThrows("if (false) { '17' **= 10; }", SyntaxError);
  assertThrows("if (false) { /17/ **= 10; }", SyntaxError);
  assertThrows("if (false) { ({ valueOf() { return 17; } } **= 10); }",
               SyntaxError);
  // TODO(caitp): a Call expression as LHS should be an early SyntaxError!
  // assertThrows("if (false) { Array() **= 10; }", SyntaxError);
  assertThrows(() => Array() **= 10, ReferenceError);
}
TestBadAssignmentLHS();
