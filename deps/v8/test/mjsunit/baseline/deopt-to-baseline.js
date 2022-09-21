// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sparkplug --no-always-sparkplug --turbofan
// Flags: --no-deopt-to-baseline

function isExecutingInterpreted(func) {
  let opt_status = %GetOptimizationStatus(func);
  return (opt_status & V8OptimizationStatus.kTopmostFrameIsInterpreted) !== 0;
}

function f(check = false) {
  if (check) {
    %DeoptimizeFunction(f);
    assertTrue(isExecutingInterpreted(f));
  }
}

f();
%CompileBaseline(f);
f();
assertTrue(isBaseline(f));

%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
assertOptimized(f);

f(true);
