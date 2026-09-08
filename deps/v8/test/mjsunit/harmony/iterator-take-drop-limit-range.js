// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let closed = false;
function createMockIterator() {
  return {
    next() { return { value: 1, done: false }; },
    return() { closed = true; return { done: true }; }
  };
}

// 1. Test take() with finite limit > 2^53 - 1
closed = false;
let iter1 = createMockIterator();
assertThrows(() => Iterator.prototype.take.call(iter1, 2 ** 53), RangeError);
assertTrue(closed);

// 2. Test drop() with finite limit > 2^53 - 1
closed = false;
let iter2 = createMockIterator();
assertThrows(() => Iterator.prototype.drop.call(iter2, 9007199254740992), RangeError);
assertTrue(closed);

// 3. Infinity should still be accepted and not trigger RangeError
closed = false;
let iter3 = createMockIterator();
let res3 = Iterator.prototype.take.call(iter3, Infinity);
assertFalse(closed);
