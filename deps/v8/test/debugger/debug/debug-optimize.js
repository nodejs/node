// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --no-always-turbofan --turbo-inlining

var Debug = debug.Debug;

function f1() {
  return 1;
}

function f2() {
  return 2;
}

function f3() {
  return f1();
}

function f4() {
  return 4;
}


function optimize(f) {
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
}

optimize(f1);
optimize(f2);
optimize(f3);

Debug.setListener(function() {});

assertOptimized(f1);
assertOptimized(f2);
assertOptimized(f3);

Debug.setBreakPoint(f1, 1);

// Setting break point deoptimizes f1 and f3 (which inlines f1).
assertUnoptimized(f1);
assertOptimized(f2);
assertUnoptimized(f3);

// We can optimize with break points set.
optimize(f4);
assertOptimized(f4);

Debug.setListener(null);
