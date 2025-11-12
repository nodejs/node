// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Test behaviors when the prototype has elements */

// forEach

(function () {
  var array = [,];

  function increment(v, k, arr) { arr[k] = v + 1; }
  function forEach() {
    array.forEach(increment);
  }

  forEach();
  assertEquals(array, [,]);

  // behavior from the prototype
  array.__proto__.push(5);
  assertEquals(Object.getOwnPropertyDescriptor(array, 0), undefined);
  forEach();
  assertNotEquals(Object.getOwnPropertyDescriptor(array, 0), undefined);
  assertEquals(array[0], 6);
})();
