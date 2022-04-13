// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

/* Test deopt behaviors when the prototype has elements */

// every

(function () {
  var array = [,];

  function every() {
    return array.every(v => v > 0);
  }

  %PrepareFunctionForOptimization(every);
  every(); every();

  %OptimizeFunctionOnNextCall(every);
  assertEquals(every(), true);

  array.__proto__.push(-6);
  //deopt
  assertEquals(every(), false);
})();
