// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  let s = "my string";
  function indexOf() {
    return s.indexOf("string");
  }
  function includes() {
    return s.includes("string");
  }
  %PrepareFunctionForOptimization(indexOf);
  %PrepareFunctionForOptimization(includes);
  assertEquals(3, indexOf());
  assertTrue(includes());
  %OptimizeMaglevOnNextCall(indexOf);
  %OptimizeMaglevOnNextCall(includes);
  assertEquals(3, indexOf());
  assertTrue(includes());
})();

// Test with position.
(function() {
  let s = "my string";
  function indexOf() {
    return s.indexOf("string", 4);
  }
  function includes() {
    return s.includes("string", 4);
  }
  %PrepareFunctionForOptimization(indexOf);
  %PrepareFunctionForOptimization(includes);
  assertEquals(-1, indexOf());
  assertFalse(includes());
  %OptimizeMaglevOnNextCall(indexOf);
  %OptimizeMaglevOnNextCall(includes);
  assertEquals(-1, indexOf());
  assertFalse(includes());
})();

// Test with non-string search string (fallback to builtin).
(function() {
  let s = "my string 123";
  function indexOf() {
    return s.indexOf(123);
  }
  %PrepareFunctionForOptimization(indexOf);
  assertEquals(10, indexOf());
  %OptimizeMaglevOnNextCall(indexOf);
  assertEquals(10, indexOf());
})();
