// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Test behaviors when the prototype has elements */

// filter

(function () {
  var array = [,];

  function filter() {
    return array.filter(v => v > 0);
  }

  assertEquals(filter(), []);

  array.__proto__.push(6);
  var narr = filter();
  assertNotEquals(Object.getOwnPropertyDescriptor(narr, 0), undefined);
  assertEquals(narr, [6]);
})();
