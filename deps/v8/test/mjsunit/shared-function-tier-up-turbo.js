// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --mark-shared-functions-for-tier-up --allow-natives-syntax
// Flags: --opt --no-always-opt --turbo-filter=*

// If we are always or never optimizing it is useless.
if (isNeverOptimizeLiteMode()) {
  print("Warning: skipping test that requires optimization in Lite mode.");
  testRunner.quit(0);
}
assertFalse(isAlwaysOptimize());
assertFalse(isNeverOptimize());

(function() {
  var sum = 0;
  var i = 0;
  for (var i = 0; i < 3; ++i) {
    var f = function(x) {
      return 2 * x;
    };
    %PrepareFunctionForOptimization(f);
    sum += f(i);

    if (i == 1) {
      // f must be interpreted code.
      assertTrue(isInterpreted(f));

      // Run twice (i = 0, 1), then tier-up.
      %OptimizeFunctionOnNextCall(f);
    } else if (i == 2) {
      // Tier-up at i = 2 should go up to turbofan.
      assertTrue(isTurboFanned(f));
    }
  }
})()
