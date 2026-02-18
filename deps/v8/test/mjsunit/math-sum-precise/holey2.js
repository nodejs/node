// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-sum-precise

(function() {
  const arr = [1, , 3];
  Object.setPrototypeOf(arr, {
    1: 100,
    [Symbol.iterator]: Array.prototype[Symbol.iterator]
  });
  assertEquals(104, Math.sumPrecise(arr));
})();

// Test subclass
(function() {
  class MyArray extends Array {}
  const arr = new MyArray(1, 2, 3);
  assertEquals(6, Math.sumPrecise(arr));
  const holey = new MyArray(3);
  holey[0] = 1;
  holey[2] = 3;
  MyArray.prototype[1] = 5;
  assertEquals(9, Math.sumPrecise(holey));
})();

(function() {
  const arr = [1.5, , 3.5];
  const proto = [, 2.5];
  arr.__proto__ = proto;
  assertEquals(7.5, Math.sumPrecise(arr));
})();
