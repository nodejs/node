// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var deepEquals;
assertEquals = function assertEquals() {
  if (!deepEquals()) {
    fail( found, name_opt);
  }
};
assertTrue = function assertTrue() {
};
assertFalse = function assertFalse() {
  assertEquals();
};

function foo(__v_7, __v_8, __v_9 = []) {
    for (const __v_10 of __v_8) {
      assertTrue(__v_7.test(), `${__v_7}.test(${__v_10})`);
    }
    for (const __v_11 of __v_9) {
      try {
        assertFalse(__v_7.__v_11, `${__v_7}.test(${__v_11})`);
      } catch (e) {}
    }
}

%PrepareFunctionForOptimization(assertEquals);
%PrepareFunctionForOptimization(assertTrue);
%PrepareFunctionForOptimization(assertFalse);
%PrepareFunctionForOptimization(foo);
foo(/(?i:ba)r/, [], [ 'BaR']);
%OptimizeFunctionOnNextCall(foo);
foo(/(?i:ba)r/, [], [ 'BaR']);
