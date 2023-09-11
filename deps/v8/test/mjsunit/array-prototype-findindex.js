// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Test behaviors when the prototype has elements */

// findIndex

(function () {
  var array = [,];

  function findIndex() {
    return array.findIndex(v => v > 0);
  }

  assertEquals(findIndex(), -1);

  array.__proto__.push(6);
  assertEquals(findIndex(), 0);
})();
