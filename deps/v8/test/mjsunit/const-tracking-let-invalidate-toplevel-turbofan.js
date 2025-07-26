// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --script-context-cells --allow-natives-syntax
// Flags: --turbofan --no-always-turbofan --maglev --no-stress-maglev
// Flags: --sparkplug --no-always-sparkplug

let a = 1;

function foo() {
  return a;
}

%PrepareFunctionForOptimization(foo);
assertEquals(1, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(1, foo());
assertOptimized(foo);

// Tier up to Turbofan.
const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

// Need a back edge for OSR to happen:
for (let i = 0; i < 3; ++i) {
  %OptimizeOsr();
}

// Change what `a` is. This is executed in Turbofan.
a = 2;

assertUnoptimized(foo);
assertEquals(2, foo());
