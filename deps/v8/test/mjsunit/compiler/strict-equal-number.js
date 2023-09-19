// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Make sure that we don't incorrectly truncate Oddball
// to Number for strict equality comparisons.
(function() {
  function foo(x, y) { return x === y; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(0.1, 0.1));
  assertTrue(foo(undefined, undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(undefined, undefined));
})();
