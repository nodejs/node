// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Test behaviors when the prototype has elements */

// includes

(function() {
  const iarr = [,3];

  function includes(arr, val) {
    return arr.includes(val);
  }

  assertFalse(includes(iarr, 2));
  assertTrue(includes(iarr, 3));

  iarr.__proto__ = [2];
  assertTrue(includes(iarr, 2));
})();

// This pollutes the Array prototype, so we should not run more tests
// in the same environment after this.
(function () {
  var array = [,];

  function includes(val) {
    return array.includes(val);
  }

  assertFalse(includes(6));

  array.__proto__.push(6);
  assertTrue(includes(6));
})();
