// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

(function test() {
  function foo(a) { a.push(a.length = 2); }

  %PrepareFunctionForOptimization(foo);
  foo([1]);
  foo([1]);
  %OptimizeFunctionOnNextCall(foo);
  foo([1]);
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo([1]);
  assertOptimized(foo);
})();

(function testElementTypeCheckSmi() {
  function foo(a) { a.push('a'); }

  %PrepareFunctionForOptimization(foo);
  foo([1]);
  foo([1]);
  %OptimizeFunctionOnNextCall(foo);
  foo([1]);
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo([1]);
  assertOptimized(foo);
})();

(function testElementTypeCheckDouble() {
  function foo(a) { a.push('a'); }

  %PrepareFunctionForOptimization(foo);
  foo([0.3413312]);
  foo([0.3413312]);
  %OptimizeFunctionOnNextCall(foo);
  foo([0.3413312]);
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo([0.3413312]);
  assertOptimized(foo);
})();
(function test() {
  function bar(a) { a.x = 2 };
  %NeverOptimizeFunction(bar);
  function foo(a) { a.push(bar(a)); }

  %PrepareFunctionForOptimization(foo);
  foo(["1"]);
  foo(["1"]);
  %OptimizeFunctionOnNextCall(foo);
  foo(["1"]);
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo(["1"]);
  assertOptimized(foo);
})();

(function test() {
  function foo(a) { a.push(a.length = 2); }

  %PrepareFunctionForOptimization(foo);
  foo([0.34234]);
  foo([0.34234]);
  %OptimizeFunctionOnNextCall(foo);
  foo([0.34234]);
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo([0.34234]);
  assertOptimized(foo);
})();

(function test() {
  const N = 128 * 1024;

  function foo(a) { a.push(1); }

  %PrepareFunctionForOptimization(foo);
  foo(new Array(N));
  foo(new Array(N));
  %OptimizeFunctionOnNextCall(foo);
  foo(new Array(N));
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo(new Array(N));
  assertOptimized(foo);
})();

(function test() {
  // Conservative arrays lengths in slow and fast mode.
  const kFastModeLength = 1024;
  const kSlowModeLength = 512 * 1024;
  function mkArray(length) {
    let a = [0.1];
    a.length = length;
    return a;
  }
  function foo(a) { a.push(0.23441233123); }


  // 1. Optimize foo to handle fast mode arrays.
  %PrepareFunctionForOptimization(foo);
  foo(mkArray(kFastModeLength));
  foo(mkArray(kFastModeLength));
  %OptimizeFunctionOnNextCall(foo);
  foo(mkArray(kFastModeLength));
  assertOptimized(foo);

  // Prepare foo to be re-optimized, ensuring it's bytecode / feedback vector
  // doesn't get flushed after deoptimization.
  %PrepareFunctionForOptimization(foo);

  // 2. Given a slow mode array, foo will deopt.
  foo(mkArray(kSlowModeLength));

  // 3. Optimize foo again.
  %OptimizeFunctionOnNextCall(foo);
  foo(mkArray(kSlowModeLength));
  // 4. It should stay optimized.
  assertOptimized(foo);
})();
