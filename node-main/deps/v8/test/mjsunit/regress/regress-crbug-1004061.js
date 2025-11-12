// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function testPackedDoublesIncludes() {
  arr = [1.5, 2.5];
  arr.length = 0;
  function f() {
    return arr.includes(1);
  };
  %PrepareFunctionForOptimization(f);
  assertEquals(f(), false);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(f(), false);
})();

(function testHoleyDoublesIncludes() {
  arr = [1.1];
  arr[3]= 1.5;
  arr.length = 0;
  function f() {
    return arr.includes(1);
  };
  %PrepareFunctionForOptimization(f);
  assertEquals(f(), false);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(f(), false);
})();

(function testPackedDoublesIndexOf() {
  arr = [1.5, 2.5];
  arr.length = 0;
  function f() {
    return arr.indexOf(1);
  };
  %PrepareFunctionForOptimization(f);
  assertEquals(f(), -1);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(f(), -1);
})();

(function testHoleyDoublesIndexOf() {
  arr = [1.1];
  arr[3]= 1.5;
  arr.length = 0;
  function f() {
    return arr.indexOf(1);
  };
  %PrepareFunctionForOptimization(f);
  assertEquals(f(), -1);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(f(), -1);
})();
