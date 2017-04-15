// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --mark-shared-functions-for-tier-up --allow-natives-syntax --ignition-staging --no-turbo

(function() {
  var sum = 0;
  var i = 0;
  for (var i = 0; i < 5; ++i) {
    var f = function(x) {
      return 2 * x;
    }
    sum += f(i);

    if (%GetOptimizationStatus(f) == 3 || %GetOptimizationStatus(f) == 4) {
      // If we are always or never optimizing f, just exit, this test is useless.
      return;
    }

    if (i == 1) {
      // f must be interpreted code.
      assertEquals(8, %GetOptimizationStatus(f));

      // Allow it to run twice (i = 0, 1), then tier-up to baseline.
      %BaselineFunctionOnNextCall(f);
    } else if (i == 2) {
      // Tier-up at i = 2 should only go up to baseline.
      assertEquals(2, %GetOptimizationStatus(f));
    } else if (i == 3) {
      // Now f must be baseline code.
      assertEquals(2, %GetOptimizationStatus(f));

      // Run two more times (i = 2, 3), then tier-up to optimized.
      %OptimizeFunctionOnNextCall(f);
    } else if (i == 4) {
      // Tier-up at i = 4 should now go up to crankshaft.
      assertEquals(1, %GetOptimizationStatus(f));
    }
  }
})()
