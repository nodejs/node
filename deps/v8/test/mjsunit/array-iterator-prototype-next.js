// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

/* Test behaviors when the prototype has elements */

(function () {
  var array = [,];

  function next() {
    return array[Symbol.iterator]().next();
  }

  assertEquals(next().value, undefined);

  array.__proto__.push(5);
  assertEquals(next().value, 5);
})();
