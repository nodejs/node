// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Basic loop peeling test case with Array.prototype.every().
(function() {
  function foo(a, o) {
    return a.every(x => x === o.x);
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo([3, 3, 3], {x:3}));
  assertFalse(foo([3, 3, 2], {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo([3, 3, 3], {x:3}));
  assertFalse(foo([3, 3, 2], {x:3}));

  // Packed
  // Non-extensible array
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.preventExtensions(['3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.preventExtensions(['3', '3', '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.preventExtensions(['3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.preventExtensions(['3', '3', '2']), {x:'3'}));

  // Sealed array
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.seal(['3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.seal(['3', '3', '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.seal(['3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.seal(['3', '3', '2']), {x:'3'}));

  // Frozen array
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.freeze(['3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.freeze(['3', '3', '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.freeze(['3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.freeze(['3', '3', '2']), {x:'3'}));

  // Holey
  // Non-extensible array
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.preventExtensions([, '3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.preventExtensions([, '3', '3', '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.preventExtensions([, '3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.preventExtensions([, '3', '3', '2']), {x:'3'}));

  // Sealed array
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.seal([, '3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.seal([, '3', '3', '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.seal([, '3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.seal([, '3', '3', '2']), {x:'3'}));

  // Frozen array
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(Object.freeze([, '3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.freeze([, '3', '3', '2']), {x:'3'}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object.freeze([, '3', '3', '3']), {x:'3'}));
  assertFalse(foo(Object.freeze([, '3', '3', '2']), {x:'3'}));
})();
