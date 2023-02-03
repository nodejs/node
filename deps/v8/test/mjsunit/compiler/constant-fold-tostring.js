// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-turbofan --turbofan

// Check that constant-folding of ToString operations works properly for NaN.
(function() {
  const foo = () => `${NaN}`;
  %PrepareFunctionForOptimization(foo);
  assertEquals("NaN", foo());
  assertEquals("NaN", foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("NaN", foo());
})();

// Check that constant-folding of ToString operations works properly for 0/-0.
(function() {
  const foo = x => `${x ? 0 : -0}`;
  %PrepareFunctionForOptimization(foo);
  assertEquals("0", foo(true));
  assertEquals("0", foo(false));
  assertEquals("0", foo(true));
  assertEquals("0", foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("0", foo(true));
  assertEquals("0", foo(false));
})();
