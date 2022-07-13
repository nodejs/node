// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

(()=> {
  function f(a, b, c) {
    return a.indexOf(b, c);
  };
  %PrepareFunctionForOptimization(f);
  f("abc", "de", 1);
  f("abc", "de", 1);
  %OptimizeFunctionOnNextCall(f);
  f("abc", "de", {});
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  f("abc", "de", {});
  assertOptimized(f);
})();

(()=> {
  function f(a, b, c) {
    return a.indexOf(b, c);
  };
  %PrepareFunctionForOptimization(f);
  f("abc", "de", 1);
  f("abc", "de", 1);
  %OptimizeFunctionOnNextCall(f);
  f("abc", {}, 1);
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  f("abc", {}, 1);
  assertOptimized(f);
})();

(()=> {
  function f(a, b, c) {
    return a.substring(b, c);
  };
  %PrepareFunctionForOptimization(f);
  f("abcde", 1, 4);
  f("abcde", 1, 4);
  %OptimizeFunctionOnNextCall(f);
  f("abcde", 1, {});
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  f("abcde", 1, {});
  assertOptimized(f);
})();

(()=> {
  function f(a, b, c) {
    return a.substring(b, c);
  };
  %PrepareFunctionForOptimization(f);
  f("abcde", 1, 4);
  f("abcde", 1, 4);
  %OptimizeFunctionOnNextCall(f);
  f("abcde", {}, 4);
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  f("abcde", {}, 4);
  assertOptimized(f);
})();
