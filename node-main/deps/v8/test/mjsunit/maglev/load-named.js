// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Checks simple monomorphic load of a Smi field works
(function() {
  function load(o) {
    return o.smi;
  }

  %PrepareFunctionForOptimization(load);
  assertEquals(42, load({smi:42}));

  %OptimizeMaglevOnNextCall(load);
  assertEquals(42, load({smi:42}));
  assertTrue(isMaglevved(load));

  // We should deopt here.
  assertEquals(42, load({y:0, smi:42}));
  assertFalse(isMaglevved(load));
})();

// Checks simple monomorphic load of a Double field works
(function() {
  function load(o) {
    return o.float64;
  }

  %PrepareFunctionForOptimization(load);
  assertEquals(42.5, load({float64:42.5}));

  %OptimizeMaglevOnNextCall(load);
  assertEquals(42.5, load({float64:42.5}));
  assertTrue(isMaglevved(load));

  // We should deopt here.
  assertEquals(42.5, load({y:0, float64:42.5}));
  assertFalse(isMaglevved(load));
})();

// Checks simple monomorphic load of a Double field works with a float64 add.
(function() {
  function load(o) {
    return o.float64 + o.float64;
  }

  %PrepareFunctionForOptimization(load);
  assertEquals(85, load({float64:42.5}));

  %OptimizeMaglevOnNextCall(load);
  assertEquals(85, load({float64:42.5}));
  assertTrue(isMaglevved(load));

  // We should deopt here.
  assertEquals(85, load({y:0, float64:42.5}));
  assertFalse(isMaglevved(load));
})();
