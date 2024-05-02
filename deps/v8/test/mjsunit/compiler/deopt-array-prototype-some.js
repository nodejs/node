// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

/* Test deopt behaviors when the prototype has elements */

// some

(function () {
  var array = [,];

  function some() {
    return array.some(v => v > 0);
  }

  %PrepareFunctionForOptimization(some);
  some(); some();

  %OptimizeFunctionOnNextCall(some);
  assertEquals(some(), false);

  array.__proto__.push(6);
  //deopt
  assertEquals(some(), true);
})();
