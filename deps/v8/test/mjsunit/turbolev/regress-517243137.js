// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function maxfun() {
  // Big enough that the inliner picks `maxfun` over the Math.max callsite.
  let a1 = 1; let a2 = 1; let a3 = 1; let a4 = 1; let a5 = 1;
  let a6 = 1; let a7 = 1; let a8 = 1; let a9 = 1; let a10 = 1;
  let a11 = 1; let a12 = 1; let a13 = 1; let a14 = 1; let a15 = 1;
  return Math.max;
}

function minfun() {
  let a1 = 1; let a2 = 1; let a3 = 1; let a4 = 1; let a5 = 1;
  let a6 = 1; let a7 = 1; let a8 = 1; let a9 = 1; let a10 = 1;
  let a11 = 1; let a12 = 1; let a13 = 1; let a14 = 1; let a15 = 1;
  return Math.min;
}

function mycall3(f, a, b, c) { return f(a | 0, b | 0, c | 0); }
function mycall4(f, a, b, c, d) { return f(a | 0, b | 0, c | 0, d | 0); }
function mycall10(f, a, b, c, d, e, g, h, i, j, k) {
  return f(a | 0, b | 0, c | 0, d | 0, e | 0, g | 0, h | 0, i | 0, j | 0, k | 0);
}

// N-arg Math.max/min folds into N-1 chained Selects on the optimizer path.
function max3(a, b, c) { return mycall3(maxfun(), a, b, c); }
function max4(a, b, c, d) { return mycall4(maxfun(), a, b, c, d); }
function max10(a, b, c, d, e, g, h, i, j, k) {
  return mycall10(maxfun(), a, b, c, d, e, g, h, i, j, k);
}
function min10(a, b, c, d, e, g, h, i, j, k) {
  return mycall10(minfun(), a, b, c, d, e, g, h, i, j, k);
}

for (const f of [maxfun, minfun, mycall3, mycall4, mycall10,
                 max3, max4, max10, min10]) {
  %PrepareFunctionForOptimization(f);
}

assertEquals(3, max3(1, 2, 3));
assertEquals(4, max4(1, 2, 3, 4));
assertEquals(10, max10(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(1, min10(10, 9, 8, 7, 6, 5, 4, 3, 2, 1));

// Polymorphic feedback on `f` so build-time target narrowing alone doesn't fold
// the call; the optimizer pass has to reduce Math.max/min post-inline.
mycall3((a, b, c) => a + b + c, 0, 0, 0);
mycall4((a, b, c, d) => a + b + c + d, 0, 0, 0, 0);
mycall10((a) => a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

%OptimizeFunctionOnNextCall(max3);
%OptimizeFunctionOnNextCall(max4);
%OptimizeFunctionOnNextCall(max10);
%OptimizeFunctionOnNextCall(min10);

assertEquals(3, max3(1, 2, 3));
assertEquals(7, max3(7, 3, 5));
assertEquals(-3, max3(-5, -3, -4));

assertEquals(4, max4(1, 2, 3, 4));
assertEquals(9, max4(9, 3, 5, 1));
assertEquals(-3, max4(-5, -3, -4, -9));

// Longer chains (9 splices), with the extremum at the start, middle and end.
assertEquals(10, max10(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(10, max10(10, 9, 8, 7, 6, 5, 4, 3, 2, 1));
assertEquals(10, max10(1, 2, 3, 10, 5, 6, 4, 3, 2, 1));
assertEquals(-1, max10(-9, -8, -7, -6, -5, -4, -3, -2, -1, -10));

assertEquals(1, min10(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(1, min10(10, 9, 8, 7, 6, 5, 4, 3, 2, 1));
assertEquals(1, min10(9, 8, 7, 1, 5, 4, 3, 2, 6, 10));
assertEquals(-10, min10(-9, -8, -7, -6, -5, -4, -3, -2, -1, -10));
