// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --mark-shared-functions-for-tier-up --allow-natives-syntax --ignition-staging --turbo

(function() {
  var sum = 0;
  var i = 0;
  for (var i = 0; i < 3; ++i) {
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

      // Run twice (i = 0, 1), then tier-up.
      %OptimizeFunctionOnNextCall(f);
    } else if (i == 2) {
      // Tier-up at i = 2 should go up to turbofan.
      assertEquals(7, %GetOptimizationStatus(f));
    }
  }
})()
