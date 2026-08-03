// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --nolazy

(function () {
  let arr = [, 3];
  function inlined() {
  }
  function foo() {
    arr.reduce(inlined);
  };
  %PrepareFunctionForOptimization(foo);
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();
