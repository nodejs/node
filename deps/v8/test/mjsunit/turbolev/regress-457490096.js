// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --no-lazy-feedback-allocation

(function () { // Shared function context.
  let x = 0;
  function bar() {
    x = x + 1;
  }

  while (x < 10) {
    function foo() {
      var y = x; // We load 'x' from the top context.
      var z = true;
        for (var i = 1; i < 3; ++i) {
          bar(); // Inlining, we load/store 'x' again, these should
                 // alias with the previous 'x'.
          if (y + i != x) z = false;
        }
      assertTrue(z);
    }
    %PrepareFunctionForOptimization(bar);
    %PrepareFunctionForOptimization(foo);
    foo();
    %OptimizeFunctionOnNextCall(foo);
  }
})();
