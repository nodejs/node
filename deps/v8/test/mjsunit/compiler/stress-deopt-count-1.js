// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --deopt-every-n-times=0 --turbofan --no-always-turbofan

// Check that --deopt-every-n-times 0 doesn't deopt

function f(x) {
  return x + 1;
}

%PrepareFunctionForOptimization(f);
f(0);
%OptimizeFunctionOnNextCall(f);

f(1);
assertOptimized(f, undefined, undefined, false);

f(1);
assertOptimized(f, undefined, undefined, false);
