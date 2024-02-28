// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sparkplug --no-always-sparkplug --use-osr
// Flags: --turbofan --no-always-turbofan --deopt-every-n-times=0
// Flags: --maglev --maglev-osr --osr-from-maglev
// Flags: --concurrent-recompilation

function isExecutingBaseline(func) {
  let opt_status = %GetOptimizationStatus(func);
  return (opt_status & V8OptimizationStatus.kTopmostFrameIsBaseline) !== 0;
}

function f() {
  for (var i = 0; i <= 20; i++) {
    if (i == 5) {
      %BaselineOsr();
    }
    if (i > 5) {
      assertTrue(isBaseline(f));
      assertTrue(isExecutingBaseline(f));
    }
  }
}

%NeverOptimizeFunction(f);
f();

var expectedStatus = V8OptimizationStatus.kTopmostFrameIsInterpreted;

function checkTopmostFrame(func) {
  let opt_status = %GetOptimizationStatus(func);
  assertTrue ((opt_status & expectedStatus) !== 0, "Expected flag " +
      expectedStatus + " to be set in optimization status");
}

function g() {
  for (var i = 0; i <= 20; i++) {
    console.log(i)
    checkTopmostFrame(g)
    if (i == 2) {
      %BaselineOsr();
      expectedStatus = V8OptimizationStatus.kTopmostFrameIsBaseline;
    }
    if (i == 10 || i == 5) {
      %OptimizeOsr();
      // Load these to make sure maglev does not deopt before we tier up.
      let ml = V8OptimizationStatus.kTopmostFrameIsMaglev;
      let tf = V8OptimizationStatus.kTopmostFrameIsTurboFanned;
      expectedStatus = i == 5 ? ml : tf;
    }
  }
}

%PrepareFunctionForOptimization(g);
g();
