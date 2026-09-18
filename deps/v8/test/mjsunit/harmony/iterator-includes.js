// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-iterator-includes

(function TestIncludesBasic() {
  function* gen() {
    yield 42;
    yield 43;
  }
  assertTrue(gen().includes(42));
  assertTrue(gen().includes(43));
  assertFalse(gen().includes(44));
})();

(function TestIncludesBuiltinIterators() {
  const set = new Set([10, 20]);
  assertTrue(set.values().includes(20));
  assertFalse(set.values().includes(30));

  const map = new Map([[1, 'a'], [2, 'b']]);
  assertTrue(map.keys().includes(1));
  assertTrue(map.values().includes('b'));
})();

(function TestIncludesNextThrowsDoesNotClose() {
  let returnCalled = false;
  const iter = {
    [Symbol.iterator]() { return this; },
    next() { throw new Error('next failed'); },
    get return() {
      returnCalled = true;
      return () => ({ done: true });
    }
  };
  Object.setPrototypeOf(iter, Iterator.prototype);

  assertThrows(() => iter.includes(42), Error, 'next failed');
  assertFalse(returnCalled);
})();

/*
// The following test is currently disabled because it relies on normative
// spec changes (PR 3776) that have not yet been merged into the
// iterator-includes proposal. It is kept here for future reference.

(function TestsForPR3776() {
  function* gen() { yield 42; yield 43; }
  const iter = gen();

  // Non-integers should be truncated.
  assertTrue(gen().includes(43, 1.5));
  assertFalse(gen().includes(42, 1.5));

  // Negative should throw RangeError.
  assertThrows(() => iter.includes(42, -1), RangeError);

  // Large values > 2^53 - 1 should throw RangeError.
  assertThrows(() => iter.includes(42, 2 ** 53), RangeError);

  // NaN skippedElements should throw RangeError.
  assertThrows(() => iter.includes(42, NaN), RangeError);
})();
*/
