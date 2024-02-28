// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft --turboshaft-instruction-selection

function simple0() { return 42; }
%PrepareFunctionForOptimization(simple0);
assertEquals(42, simple0());
%OptimizeFunctionOnNextCall(simple0);
assertEquals(42, simple0());

function simple1(x, y) { return x + y * x - y / x; }
%PrepareFunctionForOptimization(simple1);
assertEquals(31.625, simple1(8, 3));
%OptimizeFunctionOnNextCall(simple1);
assertEquals(31.625, simple1(8, 3));

function simple2(o, a, b, c) {
  o.y = a;
  for(let i = 0; i < b; ++i) {
    o.y += c;
  }
  return o;
}
%PrepareFunctionForOptimization(simple2);
assertEquals({ y:(3 + 8 * -4) }, simple2({}, 3, 8, -4));
%OptimizeFunctionOnNextCall(simple2);
assertEquals({ y:(3 + 8 * -4) }, simple2({}, 3, 8, -4));

function simple3(x, y, o) {
  function foo(f, a) { return f(a + 1.5); }
  o.x = { o:{a: x - 3.14 * y } };
  o.y = (x / y) | 0;
  o.z = y ** x;
  for(let i = -5.0; i < 5.0; i += 0.5) {
    o.z = foo((a) => a == 3.5 ? 10 : a, o.z);
  }
  return (o.w - o.x.o.a + o.y + o.z) | 0;
}
%PrepareFunctionForOptimization(simple3);
assertEquals(354533, simple3(10.2, 3.5, {w:100}));
%OptimizeFunctionOnNextCall(simple3);
assertEquals(354533, simple3(10.2, 3.5, {w:100}));
