// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --allow-natives-syntax --no-concurrent-recompilation
// Flags: --trace-deopt --no-stress-maglev --no-always-sparkplug

function testFunction(deopt) {
  if (deopt) {
    %DeoptimizeNow();
  }
  return 42;
}

%PrepareFunctionForOptimization(testFunction);
testFunction(false);
%OptimizeFunctionOnNextCall(testFunction);
testFunction(false);
// CHECK: [marking dependent code {{.*}} ({{.*}} <SharedFunctionInfo testFunction>) {{.*}} for deoptimization, reason: for testing]
// CHECK: [bailout (kind: deopt-lazy, reason: (unknown)): begin. deoptimizing {{.*}} <JSFunction testFunction
testFunction(true);

function testOsr() {
  let sum = 0;
  for (let i = 0; i < 10; i++) {
    if (i == 2) %OptimizeOsr();
    if (i == 5) %DeoptimizeNow();
    sum += i;
  }
  return sum;
}

%PrepareFunctionForOptimization(testOsr);
// CHECK: [marking dependent code {{.*}} ({{.*}} <SharedFunctionInfo testOsr>) {{.*}} for deoptimization, reason: for testing]
// CHECK: [bailout (kind: deopt-lazy, reason: (unknown)): begin. deoptimizing {{.*}} <JSFunction testOsr
testOsr();

for (let i = 0; i < 10; i++) {
  if (i == 2) %OptimizeOsr();
  if (i == 5) %DeoptimizeNow();
}
// CHECK: [marking dependent code {{.*}} ({{.*}} <SharedFunctionInfo>) {{.*}} for deoptimization, reason: for testing]
// CHECK: [bailout (kind: deopt-lazy, reason: (unknown)): begin. deoptimizing {{.*}} <JSFunction
