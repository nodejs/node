// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This tests that SpeculativeNumberSubtract can still lower to
// Int32Sub in SimplifiedLowering, which requires some magic
// to make sure that SpeculativeNumberSubtract survives to that
// point, especially the JSTypedLowering needs to be unable
// to tell that the inputs to SpeculativeNumberAdd are not
// Number, Undefined, Null or Boolean.
(function() {
  // We need a function that has a - with feedback Number or
  // NumberOrOddball, but for whose inputs the JSTypedLowering
  // cannot reduce it to NumberSubtract (with SpeculativeToNumber
  // conversions). We achieve this utilizing an object literal
  // indirection here.
  function baz(x) {
    return {x}.x - x;
  }
  baz(null);
  baz(undefined);

  // Now we just need to truncate the result.
  function foo(x) {
    return baz(42) | 0;
  }

  assertEquals(0, foo());
  assertEquals(0, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo());
})();
