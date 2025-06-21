// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Object.prototype[1] = 153;
Object.freeze(Object.prototype);

(function TestSloppyStoreToReadOnlyProperty() {
  function foo() {
    let ar = [];
    for (let i = 0; i < 3; i++) {
      ar[i] = 42;

      if (i == 1) {
        // Attempt to overwrite read-only element should not change
        // array length.
        assertEquals(1, ar.length);
      } else {
        assertEquals(i + 1, ar.length);
      }
    }
    return ar;
  }

  assertEquals([42,153,42], foo());
  assertEquals([42,153,42], foo());
  assertEquals([42,153,42], foo());
  %PrepareFunctionForOptimization(foo);
  assertEquals([42,153,42], foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([42,153,42], foo());
})();

(function StrictStoreToReadOnlyProperty() {
  function foo() {
    "use strict";
    let ar = [];
    let threw_exception = false;
    for (let i = 0; i < 3; i++) {
      try {
        ar[i] = 42;
      } catch(e) {
        // Attempt to overwrite read-only element should throw and
        // should not change array length.
        assertTrue(i == 1);
        assertEquals(1, ar.length);
        assertInstanceof(e, TypeError);
        threw_exception = true;
      }
    }
    assertTrue(threw_exception);
    return ar;
  }

  assertEquals([42,153,42], foo());
  assertEquals([42,153,42], foo());
  assertEquals([42,153,42], foo());
  %PrepareFunctionForOptimization(foo);
  assertEquals([42,153,42], foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals([42,153,42], foo());
})();
