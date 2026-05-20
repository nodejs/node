// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This tests that NumberAdd passes on the right truncations
// even if it figures out during SimplifiedLowering that it
// can indeed do a Word32 operation (based on the feedback
// baked in for its inputs by other operators).
(function() {
  // We need a + with Number feedback to get to a NumberAdd
  // during the typed lowering pass of TurboFan's frontend.
  function foo(x, y) { return x + y; }
  foo(0.1, 0.2);
  foo(0.1, 0.2);

  // Now we need to fool TurboFan to think that it has to
  // perform the `foo(x,-1)` on Float64 values until the
  // very last moment (after the RETYPE phase of the
  // SimplifiedLowering) where it realizes that the inputs
  // and outputs of the NumberAdd allow it perform the
  // operation on Word32.
  function bar(x) {
    x = Math.trunc(foo(x - 1, 1));
    return foo(x, -1);
  }

  %PrepareFunctionForOptimization(bar);
  assertEquals(0, bar(1));
  assertEquals(1, bar(2));
  %OptimizeFunctionOnNextCall(bar);
  assertEquals(2, bar(3));
})();

// This tests that SpeculativeNumberAdd can still lower to
// Int32Add in SimplifiedLowering, which requires some magic
// to make sure that SpeculativeNumberAdd survives to that
// point, especially the JSTypedLowering needs to be unable
// to tell that the inputs to SpeculativeNumberAdd are non
// String primitives.
(function() {
  // We need a function that has a + with feedback Number or
  // NumberOrOddball, but for whose inputs the JSTypedLowering
  // cannot reduce it to NumberAdd (with SpeculativeToNumber
  // conversions). We achieve this utilizing an object literal
  // indirection here.
  function baz(x) {
    return {x}.x + x;
  }
  baz(null);
  baz(undefined);

  // Now we just need to truncate the result.
  function foo(x) {
    return baz(1) | 0;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo());
  assertEquals(2, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo());
})();
