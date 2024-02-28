// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test that cloning the empty object into an object literal with
// `null` prototype works correctly.
(function() {
  function clone(o) {
    return {...o, __proto__: null};
  }

  assertNull(Object.getPrototypeOf(clone({})));
  assertNull(Object.getPrototypeOf(clone({})));
  assertNull(Object.getPrototypeOf(clone({})));
  assertNull(Object.getPrototypeOf(clone({})));
  %PrepareFunctionForOptimization(clone);
  assertNull(Object.getPrototypeOf(clone({})));
  assertNull(Object.getPrototypeOf(clone({})));
  %OptimizeFunctionOnNextCall(clone);
  assertNull(Object.getPrototypeOf(clone({})));
})();
