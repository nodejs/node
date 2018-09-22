// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

/* Test deopt behaviors when the prototype has elements */

// forEach

(function () {
  var array = [,];

  function increment (v, k, arr) { arr[k] = v + 1; }
  function forEach() {
    array.forEach(increment);
  }
  forEach(); forEach();

  %OptimizeFunctionOnNextCall(forEach);

  forEach();
  assertEquals(array, [,]);

  array.__proto__.push(5);
  assertEquals(Object.getOwnPropertyDescriptor(array, 0), undefined);
  // deopt
  forEach();
  assertNotEquals(Object.getOwnPropertyDescriptor(array, 0), undefined);
  assertEquals(array[0], 6);  // this reads from the prototype
})();
