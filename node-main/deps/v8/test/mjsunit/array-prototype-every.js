// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Test behaviors when the prototype has elements */

// every

(function () {
  var array = [,];

  function every() {
    return array.every(v => v > 0);
  }

  assertEquals(every(), true);

  array.__proto__.push(-6);
  assertEquals(every(), false);
})();
