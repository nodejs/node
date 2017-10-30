// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function Rest0Params() {

function f(x, y) {
  return x+y;
}

function test(...rest) {
  return [rest, f.apply(null, rest)];
}

assertEquals(test(), [[], NaN]);
assertEquals(test(1), [[1], NaN])
assertEquals(test(1, 2), [[1,2], 3]);
assertEquals(test(1, 2, 3), [[1,2,3], 3]);

%OptimizeFunctionOnNextCall(test);

assertEquals(test(), [[], NaN]);
assertEquals(test(1), [[1], NaN])
assertEquals(test(1, 2), [[1,2], 3]);
assertEquals(test(1, 2, 3), [[1,2,3], 3]);
})();

(function Rest1Params() {

function f(x, y) {
  return x+y
}

function test(a, ...rest) {
  return [rest, a, f.apply(null, rest)];
}

assertEquals(test(), [[], undefined, NaN]);
assertEquals(test(1), [[], 1, NaN]);
assertEquals(test(1, 2), [[2], 1, NaN]);
assertEquals(test(1, 2, 3), [[2,3], 1, 5]);
assertEquals(test(1, 2, 3, 4), [[2,3,4], 1, 5]);

%OptimizeFunctionOnNextCall(test);

assertEquals(test(), [[], undefined, NaN]);
assertEquals(test(1), [[], 1, NaN]);
assertEquals(test(1, 2), [[2], 1, NaN]);
assertEquals(test(1, 2, 3), [[2,3], 1, 5]);
assertEquals(test(1, 2, 3, 4), [[2,3,4], 1, 5]);

})();
