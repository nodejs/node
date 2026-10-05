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

(function TestIncludesSkippedElementsValidation() {
  let closed = false;
  function createClosable() {
    closed = false;
    return {
      __proto__: Iterator.prototype,
      get next() {
        throw new Error('next should not be called');
      },
      return() {
        closed = true;
        return {};
      },
    };
  }

  assertThrows(() => createClosable().includes(42, -1), RangeError);
  assertTrue(closed);

  assertThrows(() => createClosable().includes(42, Number.MAX_SAFE_INTEGER + 1), RangeError);
  assertTrue(closed);

  assertThrows(() => createClosable().includes(42, 'invalid'), TypeError);
  assertTrue(closed);

  assertThrows(() => createClosable().includes(42, NaN), TypeError);
  assertTrue(closed);

  // Infinity is allowed and skips all elements without throwing.
  function* gen() { yield 42; yield 43; }
  assertFalse(gen().includes(42, Infinity));
})();
