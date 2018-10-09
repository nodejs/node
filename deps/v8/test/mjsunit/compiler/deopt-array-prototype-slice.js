// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

/* Test deopt behaviors when the prototype has elements */

// slice

(function () {
  var array = [,];

  function slice() {
    return array.slice();
  }

  slice(); slice();

  %OptimizeFunctionOnNextCall(slice);

  assertEquals(slice(), [,]);

  array.__proto__.push(5);
  // deopt
  var narr = slice();
  assertNotEquals(Object.getOwnPropertyDescriptor(narr, 0), undefined);
  assertEquals(narr[0], 5);
})();
