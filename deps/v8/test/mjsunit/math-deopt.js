// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

(()=> {
  function f(a) {
    return Math.abs(a);
  }
  f(1);
  f(1);
  %OptimizeFunctionOnNextCall(f);
  f("100");
  %OptimizeFunctionOnNextCall(f);
  f("100");
  assertOptimized(f);
})();

(()=> {
  function f(a) {
    return Math.min(1,a);
  }
  f(1);
  f(1);
  %OptimizeFunctionOnNextCall(f);
  f("100");
  %OptimizeFunctionOnNextCall(f);
  f("100");
  assertOptimized(f);
})();

(()=> {
  function f(a) {
    return Math.pow(a,10);
  }
  f(1);
  f(1);
  %OptimizeFunctionOnNextCall(f);
  f("100");
  %OptimizeFunctionOnNextCall(f);
  f("100");
  assertOptimized(f);
})();

(()=> {
  function f(a) {
    return Math.clz32(a);
  }
  f(1);
  f(1);
  %OptimizeFunctionOnNextCall(f);
  f("100");
  %OptimizeFunctionOnNextCall(f);
  f("100");
  assertOptimized(f);
})();

(()=> {
  function f(a) {
    return Math.imul(a, 10);
  }
  f(1);
  f(1);
  %OptimizeFunctionOnNextCall(f);
  f("100");
  %OptimizeFunctionOnNextCall(f);
  f("100");
  assertOptimized(f);
})();
