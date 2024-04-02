// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-opt

// Checks Smi add operation and deopt while untagging.
(function() {
  function add(x, y) {
    return x + y;
  }

  %PrepareFunctionForOptimization(add);
  assertEquals(3, add(1, 2));

  %OptimizeMaglevOnNextCall(add);
  assertEquals(3, add(1, 2));
  assertTrue(isMaglevved(add));

  // We should deopt here in SmiUntag.
  assertEquals(0x40000000, add(1, 0x3FFFFFFF));
  assertFalse(isMaglevved(add));
})();

// Checks when we deopt due to tagging.
(function() {
  function add(x, y) {
    return x + y;
  }

  %PrepareFunctionForOptimization(add);
  assertEquals(3, add(1, 2));

  %OptimizeMaglevOnNextCall(add);
  assertEquals(3, add(1, 2));
  assertTrue(isMaglevved(add));

  // We should deopt here in SmiTag.
  assertEquals(3.2, add(1.2, 2));
  assertFalse(isMaglevved(add));
})();
