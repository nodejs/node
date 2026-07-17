// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test variable index access to array with 1 element.
(function testOneElementArrayVariableIndex() {
  function f(i) {
    const a = new Array("first");
    return a[i];
  }

  %PrepareFunctionForOptimization(f);
  assertEquals("first", f(0));
  assertEquals("first", f(0));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("first", f(0));
})();

// Test variable index access to array with 2 elements.
(function testTwoElementArrayVariableIndex() {
  function f(i) {
    const a = new Array("first", "second");
    return a[i];
  }

  %PrepareFunctionForOptimization(f);
  assertEquals("first", f(0));
  assertEquals("second", f(1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("first", f(0));
  assertEquals("second", f(1));
})();
