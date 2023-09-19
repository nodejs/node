// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

(function() {
  function add(x, y) {
    return x + y;
  }

  %PrepareFunctionForOptimization(add);
  assertEquals(4.2, add(2.1, 2.1));

  %OptimizeMaglevOnNextCall(add);
  assertEquals(4.2, add(2.1, 2.1));
  assertTrue(isMaglevved(add));

  // We don't deopt if we use smis.
  assertEquals(42, add(22, 20));
  assertTrue(isMaglevved(add));

  // We deopt if not a number.
  assertEquals("42", add("4", "2"));
  assertFalse(isMaglevved(add));
})();

// Deopt in the second Float64Unbox when the first argument is a Smi.
(function() {
  function add(x, y) {
    return x + y;
  }

  %PrepareFunctionForOptimization(add);
  assertEquals(4.2, add(2.1, 2.1));

  %OptimizeMaglevOnNextCall(add);
  assertEquals(4.2, add(2.1, 2.1));
  assertTrue(isMaglevved(add));

  // We deopt if not a number.
  assertEquals("42", add(4, "2"));
  assertFalse(isMaglevved(add));
})();

// Deopt in the second Float64Unbox when the first argument is a double.
(function() {
  function add(x, y) {
    return x + y;
  }

  %PrepareFunctionForOptimization(add);
  assertEquals(4.2, add(2.1, 2.1));

  %OptimizeMaglevOnNextCall(add);
  assertEquals(4.2, add(2.1, 2.1));
  assertTrue(isMaglevved(add));

  // We deopt if not a number.
  assertEquals("4.2!", add(4.2, "!"));
  assertFalse(isMaglevved(add));
})();

// Emit FloatAdd through SmiAdd bytecode.
(function() {
  function inc(x) {
    return x + 1
  }

  %PrepareFunctionForOptimization(inc);
  assertEquals(4.2, inc(3.2));

  %OptimizeMaglevOnNextCall(inc);
  assertEquals(4.2, inc(3.2));
})();

// Force the input of FloatAdd to be int32.
(function() {
  function add(x, y, z) {
    return (x + y) + z;
  }

  %PrepareFunctionForOptimization(add);
  assertEquals(4.2, add(1, 3, 0.2));

  %OptimizeMaglevOnNextCall(add);
  assertEquals(4.2, add(1, 3, 0.2));
})();
