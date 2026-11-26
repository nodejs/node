// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shrinking the length and making it read-only during argument coercion must
// make the fill throw at the first index past the new length.

function makeReadOnlyLength(array, length, result = 0) {
  return {
    valueOf() {
      Object.defineProperty(array, 'length', {value: length, writable: false});
      return result;
    }
  };
}

(function TestShrinkDuringStart() {
  const a = [1, 2, 3];
  assertThrows(() => a.fill(9, makeReadOnlyLength(a, 0)), TypeError);
  assertEquals(0, a.length);
  assertFalse(Object.getOwnPropertyDescriptor(a, 'length').writable);
})();

(function TestShrinkDuringEnd() {
  const a = [1, 2, 3];
  assertThrows(() => a.fill(9, 0, makeReadOnlyLength(a, 0, 3)), TypeError);
  assertEquals(0, a.length);
})();

(function TestElementsBelowNewLengthAreFilled() {
  const a = [1, 2, 3];
  assertThrows(() => a.fill(9, makeReadOnlyLength(a, 1)), TypeError);
  assertEquals([9], a);
})();

(function TestReadOnlyLengthNotShrunk() {
  const a = [1, 2, 3];
  const o = {
    valueOf() {
      Object.defineProperty(a, 'length', {writable: false});
      return 0;
    }
  };
  assertEquals([9, 9, 9], a.fill(9, o));
})();

(function TestDoubleElements() {
  const a = [1.5, 2.5, 3.5];
  assertThrows(() => a.fill(9.5, makeReadOnlyLength(a, 0)), TypeError);
  assertEquals(0, a.length);
})();
