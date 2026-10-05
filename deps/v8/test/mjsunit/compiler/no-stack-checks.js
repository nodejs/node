// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stack-checks --allow-natives-syntax --turbofan

// Test Turbofan loop and function entry with --no-stack-checks.
function test_tf(o) {
  for (let i = 0; i < 2; ++i) {
    o.x = i;
  }
  return o.x;
}

%PrepareFunctionForOptimization(test_tf);
let obj1 = { x: 0 };
assertEquals(1, test_tf(obj1));
assertEquals(1, test_tf(obj1));
%OptimizeFunctionOnNextCall(test_tf);
assertEquals(1, test_tf(obj1));
assertOptimized(test_tf);

// Test Maglev loop and function entry with --no-stack-checks.
function test_maglev(n) {
  let s = 0;
  for (let i = 0; i < n; ++i) {
    s += i;
  }
  return s;
}

%PrepareFunctionForOptimization(test_maglev);
assertEquals(45, test_maglev(10));
assertEquals(45, test_maglev(10));
%OptimizeMaglevOnNextCall(test_maglev);
assertEquals(45, test_maglev(10));

// Test nested loops without stack checks.
function test_nested_loops(n) {
  let count = 0;
  for (let i = 0; i < n; ++i) {
    for (let j = 0; j < 3; ++j) {
      count += j;
    }
  }
  return count;
}

%PrepareFunctionForOptimization(test_nested_loops);
assertEquals(30, test_nested_loops(10));
assertEquals(30, test_nested_loops(10));
%OptimizeFunctionOnNextCall(test_nested_loops);
assertEquals(30, test_nested_loops(10));
assertOptimized(test_nested_loops);
