// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that insertion at the beginning via unshift won't crash when using a
// constructor that creates an array larger than normal. (Also values inserted
// by original constructor past the end should not survive into the result of
// unshift).
(function testUnshift() {
  a = [1];
  function f() {
    return a;
  }
  b = Array.of.call(f);
  b.unshift(2);
  assertEquals(b, [2]);
})();

// Tests that insertion past the end won't expose values previously put into the
// backing store by using a constructor that creates an array larger than normal.
(function testInsertionPastEnd() {
  a = [9,9,9,9];
  function f() {
    return a;
  }
  b = Array.of.call(f,1,2);
  b[4] = 1;
  assertEquals(b, [1, 2, undefined, undefined, 1]);
})();

// Tests that using Array.of with a constructor returning an object with an
// unwriteable length throws a TypeError.
(function testFrozenArrayThrows() {
  function f() {
    return Object.freeze([1,2,3]);
  }
  assertThrows(function() { Array.of.call(f); }, TypeError);
})();
