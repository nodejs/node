// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

// Test strings with non-ascii chars

(function() {
  function foo() {
    // no index results in index = 0
    return '\u1234'.charAt() == '\u1234';
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals(true, foo());
  assertEquals(true, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(true, foo());
  assertOptimized(foo);
})();

// General case where index is within bounds
(function() {
  function foo() {
    return "\u2600\u2600\u2600 \u26A1 \u26A1 \u2603\u2603\u2603 \u2600".charAt(2) === "\u2600";
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals(true, foo());
  assertEquals(true, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(true, foo());
  assertOptimized(foo);
})();

// Case wherein index is out of bounds (returns empty string)
(function() {
  function foo() {
    return "\u2600\u2600\u2600 \u26A1 \u26A1 \u2603\u2603\u2603 \u2600".charAt(100) === "";
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals(true, foo());
  assertEquals(true, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(true, foo());
  assertOptimized(foo);
})();

// Case wherein index is negative (returns empty string)
(function() {
  function foo() {
    return "\u2600\u2600\u2600 \u26A1 \u26A1 \u2603\u2603\u2603 \u2600".charAt(-1) === "";
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals(true, foo());
  assertEquals(true, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(true, foo());
})();
