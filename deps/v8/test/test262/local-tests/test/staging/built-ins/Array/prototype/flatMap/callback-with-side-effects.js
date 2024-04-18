// Copyright (C) 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.flatMap
description: >
  Array.prototype.flatMap is given a callback that modifies the array being
  iterated.
includes: [compareArray.js]
---*/

(function TestGrow() {
  let array = [0,1,2,3];
  function f(e) {
    array[4] = 42;
    return e;
  }
  assert.compareArray(array.flatMap(f), [0,1,2,3]);
})();

(function TestShrink() {
  let array = [0,1,2,3];
  function f(e) {
    array.length = 3;
    return e;
  }
  assert.compareArray(array.flatMap(f), [0,1,2]);
})();
