// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

/* Test behaviors when the prototype has elements */

// pop

(function () {
  var array = [, , , ,];

  function pop() {
    return array.pop();
  }

  %PrepareFunctionForOptimization(pop);

  assertEquals(pop(), undefined);
  assertEquals(pop(), undefined);

  %OptimizeFunctionOnNextCall(pop);
  assertEquals(pop(), undefined);

  array.__proto__.push(6);
  assertEquals(pop(), 6);
})();
