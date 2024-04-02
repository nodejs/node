// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function ShiftRightLogicalWithDeoptUsage() {
  function g() {}

  function f() {
    var tmp = 1264475713;
    var tmp1 = tmp - (-913041544);
    g();
    return 1 >>> tmp1;
  }

  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0, f());
})();


(function ShiftRightLogicalWithCallUsage() {
  var f = (function() {
    "use asm"
    // This is not a valid asm.js, we use the "use asm" here to
    // trigger Turbofan without deoptimization support.

    function g(x) { return x; }

    function f() {
      var tmp = 1264475713;
      var tmp1 = tmp - (-913041544);
      return g(1 >>> tmp1, tmp1);
    }

    return f;
  })();

  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0, f());
})();
