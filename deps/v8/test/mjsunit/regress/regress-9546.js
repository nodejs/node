// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// Sanity checks.
assertEquals(Math.hypot(3, 4), 5);
assertEquals(Math.hypot(1, 2, 3, 4, 5, 27), 28);

// Regress.
assertEquals(Math.hypot(Infinity, NaN), Infinity);
assertEquals(Math.hypot(NaN, 0), NaN);
assertEquals(Math.hypot(NaN, Infinity), Infinity);
assertEquals(Math.hypot(0, NaN), NaN);
assertEquals(Math.hypot(NaN, 1, 2, 3, 4, 5, 0), NaN);
assertEquals(Math.hypot(NaN, Infinity, 2, 3, 4, 5, 0), Infinity);

// Verify optimized code works as intended.
function two_hypot(a, b) {
  return Math.hypot(a, b);
}

function six_hypot(a, b, c, d, e, f) {
  return Math.hypot(a, b, c, d, e, f);
}

%PrepareFunctionForOptimization(two_hypot);
two_hypot(1, 2);
two_hypot(3, 4);
two_hypot(5, 6);
%OptimizeFunctionOnNextCall(two_hypot);
assertEquals(two_hypot(3, 4), 5);

// Regress 2 parameter case.
assertEquals(two_hypot(Infinity, NaN), Infinity);
assertEquals(two_hypot(NaN, 0), NaN);
assertEquals(two_hypot(NaN, Infinity), Infinity);
assertEquals(two_hypot(0, NaN), NaN);

// Regress many parameters case.
%PrepareFunctionForOptimization(six_hypot);
six_hypot(1, 2, 3, 4, 5, 6);
six_hypot(3, 4, 5, 6, 7, 8);
six_hypot(5, 6, 7, 8, 9, 10);
%OptimizeFunctionOnNextCall(six_hypot);
assertEquals(six_hypot(1, 2, 3, 4, 5, 27), 28);

assertEquals(six_hypot(0, 0, 0, 0, 0, 0), 0);
assertEquals(six_hypot(NaN, 1, 2, 3, 4, 5, 0), NaN);
assertEquals(six_hypot(NaN, Infinity, 2, 3, 4, 5, 0), Infinity);
assertEquals(six_hypot(1, 2, 3, 4, 5, NaN), NaN);
assertEquals(six_hypot(Infinity, 2, 3, 4, 5, NaN), Infinity);
