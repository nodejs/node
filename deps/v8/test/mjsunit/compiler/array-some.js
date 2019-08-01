// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Basic loop peeling test case with Array.prototype.some().
(function() {
  function foo(a, o) {
    return a.some(x => x === o.x);
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo([1, 2, 3], {x:3}));
  assertFalse(foo([0, 1, 2], {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo([1, 2, 3], {x:3}));
  assertFalse(foo([0, 1, 2], {x:3}));

  // Packed
  // Non-extensible
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.preventExtensions([1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.preventExtensions([0, 1, '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.preventExtensions([1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.preventExtensions([0, 1, '2']), {x:'3'}));

  // Sealed
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.seal([1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.seal([0, 1, '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.seal([1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.seal([0, 1, '2']), {x:'3'}));

  // Frozen
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.freeze([1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.freeze([0, 1, '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.freeze([1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.freeze([0, 1, '2']), {x:'3'}));

  // Holey
  // Non-extensible
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.preventExtensions([, 1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.preventExtensions([, 0, 1, '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.preventExtensions([, 1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.preventExtensions([, 0, 1, '2']), {x:'3'}));

  // Sealed
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.seal([, 1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.seal([, 0, 1, '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.seal([, 1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.seal([, 0, 1, '2']), {x:'3'}));

  // Frozen
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.freeze([, 1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.freeze([, 0, 1, '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.freeze([, 1, 2, '3']), {x:'3'}));
  assertFalse(foo(Object.freeze([, 0, 1, '2']), {x:'3'}));
})();
