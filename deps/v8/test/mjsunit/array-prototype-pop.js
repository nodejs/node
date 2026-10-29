// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Test behaviors when the prototype has elements */

// pop

(function () {
  var array = [,];

  function pop() {
    return array.pop();
  }

  assertEquals(pop(), undefined);
})();


(function () {
  var array = [,];

  function pop() {
    return array.pop();
  }

  array.__proto__.push(6);
  assertEquals(pop(), 6);
})();
