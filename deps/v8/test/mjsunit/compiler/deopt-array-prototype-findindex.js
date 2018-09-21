// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

/* Test deopt behaviors when the prototype has elements */

// findIndex

(function () {
  var array = [,];

  function findIndex() {
    return array.findIndex(v => v > 0);
  }

  findIndex(); findIndex();

  %OptimizeFunctionOnNextCall(findIndex);
  assertEquals(findIndex(), -1);

  array.__proto__.push(6);
  // deopt
  assertEquals(findIndex(), 0);
})();
