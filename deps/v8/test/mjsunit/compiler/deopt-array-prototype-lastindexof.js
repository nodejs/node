// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

/* Test deopt behaviors when the prototype has elements */

// lastIndexOf

(function () {
  var array = [,];

  function lastIndexOf(val) {
    return array.lastIndexOf(val);
  }

  %PrepareFunctionForOptimization(lastIndexOf);
  lastIndexOf(6); lastIndexOf(6);

  %OptimizeFunctionOnNextCall(lastIndexOf);
  assertEquals(lastIndexOf(6), -1);

  array.__proto__.push(6);
  // deopt
  assertEquals(lastIndexOf(6), 0);
})();
