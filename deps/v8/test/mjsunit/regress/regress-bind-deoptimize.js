// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

(function() {
  function bla(x) {
    return this[x];
  }

  function foo(f) {
    return bla.bind(f())(0);
  };

  %PrepareFunctionForOptimization(foo);
  foo(() => { return [true]; });
  foo(() => { return [true]; });
  %OptimizeFunctionOnNextCall(foo);
  foo(() => { return [true]; });
  assertOptimized(foo);
  foo(() => { bla.a = 1; return [true]; });
  assertUnoptimized(foo);
})();
