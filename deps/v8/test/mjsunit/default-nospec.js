// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

(()=> {
  function f(a, b, c) {
    return String.prototype.indexOf.call(a, b, c);
  }
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
    return String.prototype.indexOf.apply(a, [b, c]);
  }
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
    return Reflect.apply(String.prototype.indexOf, a, [b, c]);
  }
  %PrepareFunctionForOptimization(f);
  f("abc", "de", 1);
  f("abc", "de", 1);
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  f({}, "de", 1);
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  f({}, "de", 1);
  assertOptimized(f);
})();

(()=> {
  function f(a, b) {
    return String.fromCharCode.call(a, b);
  }
  %PrepareFunctionForOptimization(f);
  f("abc", 1);
  f("abc", 1);
  %OptimizeFunctionOnNextCall(f);
  f("abc", {});
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  f({}, {});
  assertOptimized(f);
})();

(()=> {
  function f(a, b) {
    return String.fromCharCode.apply(undefined, [b, {}]);
  }
  %PrepareFunctionForOptimization(f);
  f("abc", 1);
  f("abc", 1);
  %OptimizeFunctionOnNextCall(f);
  f("abc", {});
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  f("abc", {});
  assertOptimized(f);
})();


(()=> {
  function f(a, b) {
    return Reflect.apply(String.fromCharCode, a, [b, {}]);
  }
  %PrepareFunctionForOptimization(f);
  f("abc", 1);
  f("abc", 1);
  %OptimizeFunctionOnNextCall(f);
  f("abc", {});
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
  f("abc", {});
  assertOptimized(f);
})();
