// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


(function() {
  const arr = new Uint32Array([2**31]);
  function foo() {
    return (arr[0] ^ 0) + 1;
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals(-(2**31) + 1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-(2**31) + 1, foo());
})();


// The remaining tests already passed without the bugfix.


(function() {
  const arr = new Uint16Array([2**15]);
  function foo() {
    return (arr[0] ^ 0) + 1;
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals(2**15 + 1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2**15 + 1, foo());
})();


(function() {
  const arr = new Uint8Array([2**7]);
  function foo() {
    return (arr[0] ^ 0) + 1;
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals(2**7 + 1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2**7 + 1, foo());
})();


(function() {
  const arr = new Int32Array([-(2**31)]);
  function foo() {
    return (arr[0] >>> 0) + 1;
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals(2**31 + 1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2**31 + 1, foo());
})();
