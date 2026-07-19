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

function f1() { return NaN; }
test((0/0), f1);

function f2() { return (0/0); }
test((0/0), f2);

function f3() { return (0/0) == (0/0); }
test(false, f3);

function f4() { return (0/0) == NaN; }
test(false, f4);

function f5() { return NaN == (0/0); }
test(false, f5);

function f6() { return "" + NaN; }
test("NaN", f6);

function f7() { return (0/0) === (0/0); }
test(false, f7);

function f8() { return (0/0) === NaN; }
test(false, f8);

function f9() { return NaN === (0/0); }
test(false, f9);

delete NaN;

function g1() { return NaN; }
test((0/0), g1);

function g2() { return (0/0); }
test((0/0), g2);

function g3() { return (0/0) == (0/0); }
test(false, g3);

function g4() { return (0/0) == NaN; }
test(false, g4);

function g5() { return NaN == (0/0); }
test(false, g5);

function g6() { return "" + NaN; }
test("NaN", g6);

function g7() { return (0/0) === (0/0); }
test(false, g7);

function g8() { return (0/0) === NaN; }
test(false, g8);

function g9() { return NaN === (0/0); }
test(false, g9);

NaN = 111;

function h1() { return NaN; }
test((0/0), h1);

function h2() { return (0/0); }
test((0/0), h2);

function h3() { return (0/0) == (0/0); }
test(false, h3);

function h4() { return (0/0) == NaN; }
test(false, h4);

function h5() { return NaN == (0/0); }
test(false, h5);

function h6() { return "" + NaN; }
test("NaN", h6);

function h7() { return (0/0) === (0/0); }
test(false, h7);

function h8() { return (0/0) === NaN; }
test(false, h8);

function h9() { return NaN === (0/0); }
test(false, h9);

// -------------

function k1() { return this.NaN; }
test((0/0), k1);

function k2() { return (0/0); }
test((0/0), k2);

function k3() { return (0/0) == (0/0); }
test(false, k3);

function k4() { return (0/0) == this.NaN; }
test(false, k4);

function k5() { return this.NaN == (0/0); }
test(false, k5);

function k6() { return "" + this.NaN; }
test("NaN", k6);

function k7() { return (0/0) === (0/0); }
test(false, k7);

function k8() { return (0/0) === this.NaN; }
test(false, k8);

function k9() { return this.NaN === (0/0); }
test(false, k9);
