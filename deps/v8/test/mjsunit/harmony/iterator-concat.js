// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-iterator-sequencing

function TestHelperPrototypeSurface(helper) {
  const proto = Object.getPrototypeOf(helper);
  assertEquals('Iterator Helper', proto[Symbol.toStringTag]);
  assertTrue(Object.hasOwn(proto, 'next'));
  assertTrue(Object.hasOwn(proto, 'return'));
  assertEquals('function', typeof proto.next);
  assertEquals('function', typeof proto.return);
  assertEquals(0, proto.next.length);
  assertEquals(0, proto.return.length);
  assertEquals('next', proto.next.name);
  assertEquals('return', proto.return.name);
}

// --- Test concat helper

(function TestConcat() {
  assertEquals('function', typeof Iterator.concat);
  assertEquals(0, Iterator.concat.length);
  assertEquals('concat', Iterator.concat.name);

  const iter1 = [1, 2].values();
  const iter2 = new Set([3, 4]).values();
  const iter3 = [5, 6];
  const concatIter = Iterator.concat(iter1, iter2, iter3);
  TestHelperPrototypeSurface(concatIter);
  assertEquals([1, 2, 3, 4, 5, 6], [...concatIter]);
})();

(function TestConcatWithEmpty() {
  const iter1 = [1, 2].values();
  const iter2 = [].values();
  const iter3 = [3, 4];
  const concatIter = Iterator.concat(iter1, iter2, iter3);
  TestHelperPrototypeSurface(concatIter);
  assertEquals([1, 2, 3, 4], [...concatIter]);
})();

(function TestConcatMultipleEmpty() {
  const iter1 = [1, 2].values();
  const iter2 = [].values();
  const iter3 = new Set([]).values();
  const iter4 = [3, 4];
  const concatIter = Iterator.concat(iter1, iter2, iter3, iter4);
  TestHelperPrototypeSurface(concatIter);
  assertEquals([1, 2, 3, 4], [...concatIter]);
})();

(function TestConcatOnlyEmpty() {
  const iter1 = [].values();
  const iter2 = new Set([]).values();
  const concatIter = Iterator.concat(iter1, iter2);
  TestHelperPrototypeSurface(concatIter);
  assertEquals([], [...concatIter]);
})();

(function TestConcatNonIterable() {
  assertThrows(() => Iterator.concat([1].values(), 42), TypeError);
  assertThrows(() => Iterator.concat({}, [1].values()), TypeError);
  assertThrows(() => Iterator.concat([1].values(), null), TypeError);
})();

(function TestConcatReturn() {
  let returnCount1 = 0;
  const iter1 = {
    i: 1,
    next() { return { value: this.i++, done: false }; },
    return() {
      returnCount1++;
      return { done: true };
    }
  };
  Object.setPrototypeOf(iter1, Iterator.prototype);

  let returnCount2 = 0;
  const iter2 = {
    i: 1,
    next() { return { value: this.i++, done: false }; },
    return() {
      returnCount2++;
      return { done: true };
    }
  };
  Object.setPrototypeOf(iter2, Iterator.prototype);

  const concatIter = Iterator.concat(iter1, iter2);
  assertEquals({ value: 1, done: false }, concatIter.next());
  assertEquals(0, returnCount1);
  assertEquals(0, returnCount2);

  concatIter.return();

  assertEquals(1, returnCount1);
  assertEquals(0, returnCount2);
})();

(function TestConcatThrowInInner() {
  let returnCount = 0;
  const iter1 = {
    i: 1,
    next() {
      if (this.i == 2) {
        throw new Error("test error");
      }
      return { value: this.i++, done: false };
    },
    return() {
      returnCount++;
      return { done: true };
    }
  };
  Object.setPrototypeOf(iter1, Iterator.prototype);

  const iter2 = [3, 4].values();

  const concatIter = Iterator.concat(iter1, iter2);
  assertEquals({ value: 1, done: false }, concatIter.next());
  assertEquals(0, returnCount);
  assertThrows(() => concatIter.next());
  assertEquals(1, returnCount);
  assertEquals({ value: undefined, done: true }, concatIter.next());
})();

(function TestConcatLazyIteratorCreation() {
  let iteratorMethodCalled = false;
  const iterableWithSideEffect = {
    [Symbol.iterator]() {
      iteratorMethodCalled = true;
      return [1, 2].values();
    }
  };

  const iter = Iterator.concat(iterableWithSideEffect);
  assertFalse(iteratorMethodCalled);
  assertEquals([1, 2], [...iter]);
  assertTrue(iteratorMethodCalled);
})();

(function TestConcatLazyIteratorCreationThrows() {
  let iteratorMethodThrows = false;
  const iterableThrows = {
    [Symbol.iterator]() {
      iteratorMethodThrows = true;
      throw new Error("@@iterator error");
    }
  };

  const iter = Iterator.concat(iterableThrows);
  assertFalse(iteratorMethodThrows);
  assertThrows(() => iter.next(), Error, "@@iterator error");
  assertTrue(iteratorMethodThrows);
})();

(function TestConcatNonObjectItemThrowsTypeError() {
  assertThrows(() => Iterator.concat("abc"), TypeError,
               "Iterator.concat called on non-object");
  assertThrows(() => Iterator.concat(123), TypeError,
               "Iterator.concat called on non-object");
  assertThrows(() => Iterator.concat(true), TypeError,
               "Iterator.concat called on non-object");
  assertThrows(() => Iterator.concat(Symbol('foo')), TypeError,
               "Iterator.concat called on non-object");
  assertThrows(() => Iterator.concat(null), TypeError,
               "Iterator.concat called on non-object");
  assertThrows(() => Iterator.concat(undefined), TypeError,
               "Iterator.concat called on non-object");
})();

(function TestConcatWithIterableThatIsNotAnIterator() {
  const iterable = {
    [Symbol.iterator]() {
      return [1, 2].values();
    }
  };

  const concatIter = Iterator.concat(iterable);
  assertEquals([1, 2], [...concatIter]);
})();

(function TestConcatHelperCleanupOnReturnError() {
  const baseIterator = {
    i: 0,
    next() { return { value: this.i++, done: false }; },
    return() {
      throw new Error("Custom return error");
    }
  };

  const baseIterable = {
    [Symbol.iterator]() { return baseIterator; }
  };

  const concatIterator = Iterator.concat(baseIterable);

  // Advance the iterator once to activate it.
  concatIterator.next();

  // The first call to .return() should throw the custom error.
  assertThrows(() => {
    concatIterator.return();
  }, Error, "Custom return error");

  // After the error, the helper should be in an "exhausted" state.
  // A subsequent call to .next() should not throw.
  const result = concatIterator.next();
  assertEquals(undefined, result.value);
  assertTrue(result.done);
})();

(function TestConcatBasic() {
  assertEquals([], Array.from(Iterator.concat()));
  assertEquals([0, 1, 2], Array.from(Iterator.concat([0, 1, 2])));
  assertEquals([0, 1, 2, 3, 4, 5],
               Array.from(Iterator.concat([0, 1, 2], [3, 4, 5])));
  assertEquals([0, 1, 2, 3, 4, 5, 6, 7, 8],
               Array.from(Iterator.concat([0, 1, 2], [3, 4, 5], [6, 7, 8])));
})();

(function TestConcatDoesNotIterateStrings() {
  assertThrows(() => Iterator.concat("ab", "cd"), TypeError);
})();

(function TestConcatClosesOpenIterators_StoppedBetween() {
  let returned = [];
  let concatted = Iterator.concat(
    [0, 1],
    [2, 3],
    {
      [Symbol.iterator]() {
        return {
          next() {
            return { done: false, value: 4 };
          },
          return() {
            returned.push('a');
            return { done: true, value: undefined };
          },
        };
      },
    },
  );
  assertEquals(concatted.next().value, 0);
  assertEquals(concatted.next().value, 1);
  assertEquals(concatted.next().value, 2);
  assertEquals(concatted.next().value, 3);
  assertEquals([], returned);
  concatted.return();
  assertEquals([], returned);
})();

(function TestConcatClosesOpenIterators_StoppedDuring() {
  let returned = [];
  let concatted = Iterator.concat(
    [0, 1],
    [2, 3],
    {
      [Symbol.iterator]() {
        return {
          next() {
            return { done: false, value: 4 };
          },
          return() {
            returned.push('a');
            return { done: true, value: undefined };
          },
        };
      },
    },
    {
      [Symbol.iterator]() {
        return {
          next() {
            return { done: false, value: 'b' };
          },
          return() {
            returned.push('b');
            return { done: true, value: undefined };
          },
        };
      },
    },
  );
  assertEquals(concatted.next().value, 0);
  assertEquals(concatted.next().value, 1);
  assertEquals(concatted.next().value, 2);
  assertEquals(concatted.next().value, 3);
  assertEquals(concatted.next().value, 4);
  assertEquals([], returned);
  concatted.return();
  assertEquals(['a'], returned);
  concatted.return();
  assertEquals(['a'], returned);
})();


(function TestConcatSuspendedStartReentrancy() {
  let helperIterator;
  const baseIterator = [1, 2, 3].values();

  baseIterator.return = function () {
    helperIterator.return();
    return { done: true };
  };

  helperIterator = Iterator.concat(baseIterator);

  // The recursive call in the "return" method should not throw a TypeError,
  // as the helper iterator is not active (it is in the SUSPENDED-START state).
  const result = helperIterator.return();
  assertEquals(undefined, result.value);
  assertTrue(result.done);
})();
