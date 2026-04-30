// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  // Case 1: Array with no Symbol.iterator (should throw TypeError)
  let arr = [1.0, 2.0, 3.0];
  arr.__proto__ = {};  // Removes Symbol.iterator from prototype chain
  assertThrows(() => Math.sumPrecise(arr), TypeError);
}

{
  // Case 2: Custom iterator is ignored (should use custom iterator)
  let arr = [10, 20, 30];
  arr.__proto__ = {
    [Symbol.iterator]() {
      let vals = [100, 200, 300];
      let i = 0;
      return {
        next() {
          if (i < vals.length) return { value: vals[i++], done: false };
          return { value: undefined, done: true };
        }
      };
    }
  };
  assertEquals(600, Math.sumPrecise(arr));
}

{
  // Case 3: Object.setPrototypeOf also triggers the bug
  let arr = [5.0, 6.0, 7.0];
  Object.setPrototypeOf(arr, {});
  assertThrows(() => Math.sumPrecise(arr), TypeError);
}

{
  // Case 4: JSArrayIterator on packed array with modified prototype.
  // The prototype should be ignored because it's packed.
  let arr = [10, 20, 30];
  arr.__proto__ = {
    get 0() { return 100; }
  };
  let iter = Array.prototype.values.call(arr);
  let obj = { [Symbol.iterator]() { return iter; } };
  assertEquals(60, Math.sumPrecise(obj));
}

{
  // Case 5: JSArrayIterator on holey array with modified prototype.
  // The prototype should be respected because it falls back to slow path.
  let arr = [10, , 30];
  arr.__proto__ = {
    get 1() { return 200; },
    __proto__: Array.prototype
  };
  let iter = Array.prototype.values.call(arr);
  let obj = { [Symbol.iterator]() { return iter; } };
  assertEquals(240, Math.sumPrecise(obj));
}
