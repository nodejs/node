// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test with-statements inside a try-catch block.
(() => {
  function f(object) {
    try {
      with (object) { return x }
    } catch(e) {
      return e
    }
  }
  assertEquals(23, f({ x:23 }));
  assertEquals(42, f({ x:42 }));
  assertInstanceof(f(null), TypeError);
  assertInstanceof(f(undefined), TypeError);
  %OptimizeFunctionOnNextCall(f);
  assertInstanceof(f(null), TypeError);
  assertInstanceof(f(undefined), TypeError);
})();
