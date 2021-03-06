// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --runtime-calls-stats --expose-gc

// Try to exercise the runtime stats code for optimization and GC.

// Optimize some functions both in the foreground and in the background.
function test(x) {
  return 1 + Math.sin(x);
}

function testConcurrent(x) {
  return 1 + Math.cos(x);
}

%PrepareFunctionForOptimization(test);
test(0.5);
test(0.6);
%OptimizeFunctionOnNextCall(test);
for (var i = 0; i < 100; ++i) {
  test(0.7);
}

%PrepareFunctionForOptimization(testConcurrent);
testConcurrent(0.5);
testConcurrent(0.6);
%OptimizeFunctionOnNextCall(testConcurrent, 'concurrent');
for (var i = 0; i < 100; ++i) {
  testConcurrent(0.7);
}

%GetOptimizationStatus(testConcurrent, 'sync');

gc();
