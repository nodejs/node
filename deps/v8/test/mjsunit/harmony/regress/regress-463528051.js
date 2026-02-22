// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestIteratorHelperReturnNotReentrant() {
  let enterCount = 0;

  // We use ".map()" to get a representative simple iterator helper.
  let mapIterator;

  const baseIterator = [1, 2, 3].values();

  baseIterator.return = function() {
    enterCount++;
    // This is the recursive call. We are inside the helper's .return(),
    // which has called the base's .return(), and now we call the
    // helper's .return() again. This should throw a TypeError.
    mapIterator.return();
    return { done: true };
  };

  mapIterator = baseIterator.map(v => v * 2);

  // Advance the iterator once. This is necessary to activate the inner
  // iterator, so that its .return() method will be called.
  mapIterator.next();

  assertThrows(function() {
    mapIterator.return();
  }, TypeError, "Generator is already running");

  assertEquals(1, enterCount);
})();

(function TestFlatMapHelperReturnNotReentrant() {
  let enterCount = 0;

  // We use ".flatMap()" which is not a simple iterator helper.
  let flatMapIterator;

  const innerIterator = {
    next() { return { done: false }; },
    return() {
      enterCount++;
      // This is the recursive call. When the flatMap helper's .return()
      // is called, it tries to close its active inner iterator. This will
      // recursively call the flatMap helper's return(), which should throw
      // a TypeError.
      flatMapIterator.return();
      return { done: true };
    }
  };

  const innerIterable = {
    [Symbol.iterator]() {
      return innerIterator;
    }
  };

  const outerIterator = [1].values();
  const mapper = () => innerIterable;

  flatMapIterator = outerIterator.flatMap(mapper);

  // Advance the iterator once. This is necessary to activate the inner
  // iterator, so that its .return() method will be called.
  flatMapIterator.next();

  assertThrows(function() {
    flatMapIterator.return();
  }, TypeError, "Generator is already running");

  assertEquals(1, enterCount);
})();

(function TestFlatMapHelperReturnInnerThrows() {
  let outerReturnCalled = false;

  const innerIterator = {
    next() { return { done: false }; },
    return() { throw new Error("Inner iterator return error"); }
  };

  const innerIterable = {
    [Symbol.iterator]() { return innerIterator; }
  };

  const outerIterator = [1].values();
  outerIterator.return = function() {
    outerReturnCalled = true;
    return { done: true };
  };

  const mapper = () => innerIterable;
  const flatMapIterator = outerIterator.flatMap(mapper);

  // Advance to activate the inner iterator.
  flatMapIterator.next();

  assertThrows(() => {
    flatMapIterator.return();
  }, Error, "Inner iterator return error");

  assertTrue(outerReturnCalled);
})();

(function TestFlatMapHelperCleanupOnReturnError() {
  const innerIterator = {
    next() { return { done: false }; },
    return() { throw new Error("Inner return error"); }
  };

  const innerIterable = {
    [Symbol.iterator]() { return innerIterator; }
  };

  const outerIterator = [1].values();
  outerIterator.return = function() { return { done: true }; };

  const mapper = () => innerIterable;
  const flatMapIterator = outerIterator.flatMap(mapper);

  // Advance to activate the inner iterator.
  flatMapIterator.next();

  // The first call to .return() should throw the inner iterator's error.
  assertThrows(() => {
    flatMapIterator.return();
  }, Error, "Inner return error");

  // After the error, the helper should be "exhausted". A subsequent call
  // to .next() should not throw a TypeError.
  const result = flatMapIterator.next();
  assertEquals(undefined, result.value);
  assertTrue(result.done);
})();

function checkSuspendedStartReentrancy(createHelper) {
  let helperIterator;
  const baseIterator = [1, 2, 3].values();

  baseIterator.return = function() {
    helperIterator.return();
    return { done: true };
  };

  helperIterator = createHelper(baseIterator);

  // The recursive call in the "return" method should not throw a TypeError,
  // as the helper iterator is not active (it is in the SUSPENDED-START state).
  const result = helperIterator.return();
  assertEquals(undefined, result.value);
  assertTrue(result.done);
}

(function TestMapSuspendedStartReentrancy() {
  checkSuspendedStartReentrancy(iter => iter.map(v => v));
})();

(function TestTakeSuspendedStartReentrancy() {
  checkSuspendedStartReentrancy(iter => iter.take(1));
})();

(function TestDropSuspendedStartReentrancy() {
  checkSuspendedStartReentrancy(iter => iter.drop(1));
})();

(function TestFlatMapSuspendedStartReentrancy() {
  checkSuspendedStartReentrancy(iter => iter.flatMap(v => [v]));
})();
