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

function f1() { return Infinity; }
test((1/0), f1);

function f2() { return (1/0); }
test((1/0), f2);

function f3() { return (1/0) == (1/0); }
test(true, f3);

function f4() { return (1/0) == Infinity; }
test(true, f4);

function f5() { return Infinity == (1/0); }
test(true, f5);

function f6() { return "" + Infinity; }
test("Infinity", f6);

function f7() { return (1/0) === (1/0); }
test(true, f7);

function f8() { return (1/0) === Infinity; }
test(true, f8);

function f9() { return Infinity === (1/0); }
test(true, f9);

delete Infinity;

function g1() { return Infinity; }
test((1/0), g1);

function g2() { return (1/0); }
test((1/0), g2);

function g3() { return (1/0) == (1/0); }
test(true, g3);

function g4() { return (1/0) == Infinity; }
test(true, g4);

function g5() { return Infinity == (1/0); }
test(true, g5);

function g6() { return "" + Infinity; }
test("Infinity", g6);

function g7() { return (1/0) === (1/0); }
test(true, g7);

function g8() { return (1/0) === Infinity; }
test(true, g8);

function g9() { return Infinity === (1/0); }
test(true, g9);

Infinity = 111;

function h1() { return Infinity; }
test((1/0), h1);

function h2() { return (1/0); }
test((1/0), h2);

function h3() { return (1/0) == (1/0); }
test(true, h3);

function h4() { return (1/0) == Infinity; }
test(true, h4);

function h5() { return Infinity == (1/0); }
test(true, h5);

function h6() { return "" + Infinity; }
test("Infinity", h6);

function h7() { return (1/0) === (1/0); }
test(true, h7);

function h8() { return (1/0) === Infinity; }
test(true, h8);

function h9() { return Infinity === (1/0); }
test(true, h9);

// -------------

function k1() { return this.Infinity; }
test((1/0), k1);

function k2() { return (1/0); }
test((1/0), k2);

function k3() { return (1/0) == (1/0); }
test(true, k3);

function k4() { return (1/0) == this.Infinity; }
test(true, k4);

function k5() { return this.Infinity == (1/0); }
test(true, k5);

function k6() { return "" + this.Infinity; }
test("Infinity", k6);

function k7() { return (1/0) === (1/0); }
test(true, k7);

function k8() { return (1/0) === this.Infinity; }
test(true, k8);

function k9() { return this.Infinity === (1/0); }
test(true, k9);
