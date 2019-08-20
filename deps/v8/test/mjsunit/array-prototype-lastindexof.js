// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Test behaviors when the prototype has elements */

// lastIndexOf

(function () {
  var array = [,];

  function lastIndexOf(val) {
    return array.lastIndexOf(val);
  }

  assertEquals(lastIndexOf(6), -1);

  array.__proto__.push(6);
  assertEquals(lastIndexOf(6), 0);
})();
