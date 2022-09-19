// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(expected, f) {
  %PrepareFunctionForOptimization(f);
  assertEquals(expected, f());
  assertEquals(expected, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected, f());
  assertEquals(expected, f());
}

function testThrows(f) {
  %PrepareFunctionForOptimization(f);
  assertThrows(f);
  assertThrows(f);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f);
  assertThrows(f);
}

function f1() { return undefined; }
test(void 0, f1);

function f2() { return void 0; }
test(void 0, f2);

function f3() { return void 0 == void 0; }
test(true, f3);

function f4() { return void 0 == undefined; }
test(true, f4);

function f5() { return undefined == void 0; }
test(true, f5);

function f6() { return "" + undefined; }
test("undefined", f6);

function f7() { return void 0 === void 0; }
test(true, f7);

function f8() { return void 0 === undefined; }
test(true, f8);

function f9() { return undefined === void 0; }
test(true, f9);

delete undefined;

function g1() { return undefined; }
test(void 0, g1);

function g2() { return void 0; }
test(void 0, g2);

function g3() { return void 0 == void 0; }
test(true, g3);

function g4() { return void 0 == undefined; }
test(true, g4);

function g5() { return undefined == void 0; }
test(true, g5);

function g6() { return "" + undefined; }
test("undefined", g6);

function g7() { return void 0 === void 0; }
test(true, g7);

function g8() { return void 0 === undefined; }
test(true, g8);

function g9() { return undefined === void 0; }
test(true, g9);

undefined = 111;

function h1() { return undefined; }
test(void 0, h1);

function h2() { return void 0; }
test(void 0, h2);

function h3() { return void 0 == void 0; }
test(true, h3);

function h4() { return void 0 == undefined; }
test(true, h4);

function h5() { return undefined == void 0; }
test(true, h5);

function h6() { return "" + undefined; }
test("undefined", h6);

function h7() { return void 0 === void 0; }
test(true, h7);

function h8() { return void 0 === undefined; }
test(true, h8);

function h9() { return undefined === void 0; }
test(true, h9);

// -------------

function k1() { return this.undefined; }
test(void 0, k1);

function k2() { return void 0; }
test(void 0, k2);

function k3() { return void 0 == void 0; }
test(true, k3);

function k4() { return void 0 == this.undefined; }
test(true, k4);

function k5() { return this.undefined == void 0; }
test(true, k5);

function k6() { return "" + this.undefined; }
test("undefined", k6);

function k7() { return void 0 === void 0; }
test(true, k7);

function k8() { return void 0 === this.undefined; }
test(true, k8);

function k9() { return this.undefined === void 0; }
test(true, k9);

// -------------

function m1() { return undefined.x; }
testThrows(m1);

function m2() { return undefined.undefined; }
testThrows(m2);

function m3() { return (void 0).x; }
testThrows(m3);

function m4() { return (void 0).undefined; }
testThrows(m4);
