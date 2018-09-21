// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

/* Test deopt behaviors when the prototype has elements */

// map

(function () {
  var array = [,];

  function map() {
    return array.map(x => x + 1);
  }

  map(); map();

  %OptimizeFunctionOnNextCall(map);

  assertEquals(map(), [,]);

  array.__proto__.push(5);
  // deopt
  var narr = map();
  assertNotEquals(Object.getOwnPropertyDescriptor(narr, 0), undefined);
  assertEquals(narr[0], 6);
})();
