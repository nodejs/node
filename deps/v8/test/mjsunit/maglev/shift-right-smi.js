// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-opt

// Checks Smi shift_right operation and deopt while untagging.
(function() {
  function shift_right(x, y) {
    return x >> y;
  }

  %PrepareFunctionForOptimization(shift_right);
  assertEquals(2, shift_right(8, 2));
  assertEquals(-2, shift_right(-8, 2));
  assertEquals(-8, shift_right(-8, 0));
  assertEquals(0, shift_right(8, 10));
  assertEquals(4, shift_right(8, 33));

  %OptimizeMaglevOnNextCall(shift_right);
  assertEquals(2, shift_right(8, 2));
  assertTrue(isMaglevved(shift_right));

  assertEquals(-2, shift_right(-8, 2));
  assertTrue(isMaglevved(shift_right));

  assertEquals(-8, shift_right(-8, 0));
  assertTrue(isMaglevved(shift_right));

  assertEquals(0, shift_right(8, 10));
  assertTrue(isMaglevved(shift_right));

  // Shifts are mod 32
  assertEquals(4, shift_right(8, 33));
  assertTrue(isMaglevved(shift_right));

  // // We should deopt here in SmiUntag.
  // assertEquals(0x40000000, shift_right(1, 0x3FFFFFFF));
  // assertFalse(isMaglevved(shift_right));
})();

// // Checks when we deopt due to tagging.
// (function() {
//   function shift_right(x, y) {
//     return x + y;
//   }

//   %PrepareFunctionForOptimization(shift_right);
//   assertEquals(3, shift_right(1, 2));

//   %OptimizeMaglevOnNextCall(shift_right);
//   assertEquals(3, shift_right(1, 2));
//   assertTrue(isMaglevved(shift_right));

//   // We should deopt here in SmiTag.
//   assertEquals(3.2, shift_right(1.2, 2));
//   assertFalse(isMaglevved(shift_right));
// })();
