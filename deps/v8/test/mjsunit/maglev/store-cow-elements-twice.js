// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev
// Flags: --no-optimize-on-next-call-optimizes-to-maglev

// The second store reuses the backing store made writable by the first.
function swapCow(i, j) {
  let a = [10, 20, 30, 40];
  let tmp = a[i];
  a[i] = a[j];
  a[j] = tmp;
  return a;
}

%PrepareFunctionForOptimization(swapCow);
assertEquals([20, 10, 30, 40], swapCow(0, 1));
assertEquals([10, 20, 40, 30], swapCow(2, 3));
%OptimizeFunctionOnNextCall(swapCow);
assertEquals([40, 20, 30, 10], swapCow(0, 3));
assertEquals([10, 30, 20, 40], swapCow(1, 2));
assertOptimized(swapCow);
