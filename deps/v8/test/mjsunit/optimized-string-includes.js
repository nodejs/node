// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

(function optimize() {
  function f() {
    return 'abc'.includes('a');
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f());
  assertEquals(true, f());
  assertEquals(true, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f());
  assertOptimized(f);

  function f2() {
    return 'abc'.includes('a', 1);
  }
  %PrepareFunctionForOptimization(f2);
  assertEquals(false, f2());
  assertEquals(false, f2());
  assertEquals(false, f2());
  %OptimizeFunctionOnNextCall(f2);
  assertEquals(false, f2());
  assertOptimized(f2);

  function f3() {
    return 'abc'.includes('b');
  }
  %PrepareFunctionForOptimization(f3);
  assertEquals(true, f3());
  assertEquals(true, f3());
  assertEquals(true, f3());
  %OptimizeFunctionOnNextCall(f3);
  assertEquals(true, f3());
  assertOptimized(f3);

  function f4() {
    return 'abcbc'.includes('bc', 2);
  }
  %PrepareFunctionForOptimization(f4);
  assertEquals(true, f4());
  assertEquals(true, f4());
  assertEquals(true, f4());
  %OptimizeFunctionOnNextCall(f4);
  assertEquals(true, f4());
  assertOptimized(f4);

  function f5() {
    return 'abcbc'.includes('b', -1);
  }
  %PrepareFunctionForOptimization(f5);
  assertEquals(true, f5());
  assertEquals(true, f5());
  assertEquals(true, f5());
  %OptimizeFunctionOnNextCall(f5);
  assertEquals(true, f5());
  assertOptimized(f5);

  function f6() {
    return 'abcbc'.includes('b', -10737418);
  }
  %PrepareFunctionForOptimization(f6);
  assertEquals(true, f6());
  assertEquals(true, f6());
  assertEquals(true, f6());
  %OptimizeFunctionOnNextCall(f6);
  assertEquals(true, f6());
  assertOptimized(f6);
})();

(function optimizeOSR() {
  function f() {
    var result;
    for (var i = 0; i < 100000; i++) {
      result = 'abc'.includes('a');
    }
    return result;
  }
  assertEquals(true, f());

  function f2() {
    var result;
    for (var i = 0; i < 100000; i++) {
      result = 'abc'.includes('a', 1);
    }
    return result;
  }
  assertEquals(false, f2());

  function f3() {
    var result;
    for (var i = 0; i < 100000; i++) {
      result = 'abc'.includes('b');
    }
    return result;
  }
  assertEquals(true, f3());

  function f4() {
    var result;
    for (var i = 0; i < 100000; i++) {
      result = 'abcbc'.includes('bc', 2);
    }
    return result;
  }
  assertEquals(true, f4());
})();

(function bailout() {
  function f(str) {
    return String.prototype.includes.call(str, 'a')
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(true, f('abc'));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(true, f({
                 toString: () => {
                   return 'abc'
                 }
  }));
  assertUnoptimized(f);

  function f2(str) {
    return 'abc'.includes(str)
  }
  %PrepareFunctionForOptimization(f2);
  assertEquals(true, f2('a'));
  %OptimizeFunctionOnNextCall(f2);
  assertEquals(true, f2({
                 toString: () => {
                   return 'a'
                 }
               }));
  assertUnoptimized(f2);

  function f3(index) {
    return 'abc'.includes('a', index)
  }
  %PrepareFunctionForOptimization(f3);
  assertEquals(true, f3(0));
  %OptimizeFunctionOnNextCall(f3);
  assertEquals(true, f3({
                 valueOf: () => {
                   return 0
                 }
  }));
  assertUnoptimized(f3);
})();
