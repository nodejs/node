// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


let obj = {v: 3};

(function() {
  function test_and(x) {
    return x.v & (3 << 29);
  }

  obj.v = -1;
  %PrepareFunctionForOptimization(test_and);
  const result_i = test_and(obj);

  %OptimizeFunctionOnNextCall(test_and);
  const result_c = test_and(obj);

  assertEquals(result_i, result_c);
})();

(function() {
  function test_or(x) {
    return x.v | (3 << 29);
  }

  obj.v = 1 << 29;
  %PrepareFunctionForOptimization(test_or);
  const result_i = test_or(obj);

  %OptimizeFunctionOnNextCall(test_or);
  const result_c = test_or(obj);

  assertEquals(result_i, result_c);
})();

(function() {
  function test_xor(x) {
    return x.v ^ (3 << 29);
  }

  obj.v = 1 << 29;
  %PrepareFunctionForOptimization(test_xor);
  const result_i = test_xor(obj);

  %OptimizeFunctionOnNextCall(test_xor);
  const result_c = test_xor(obj);

  assertEquals(result_i, result_c);
})();
