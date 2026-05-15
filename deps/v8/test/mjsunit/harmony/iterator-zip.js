// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-joint-iteration

(function TestPositionalShortest() {
  assertArrayEquals([
    [0, 1],
  ], Array.from(Iterator.zip([
    [0],
    [1, 2],
  ])));
})();

(function TestPositionalEquiv() {
  assertArrayEquals([
    [0, 3, 6],
    [1, 4, 7],
    [2, 5, 8],
  ], Array.from(Iterator.zip([
    [0, 1, 2],
    [3, 4, 5],
    [6, 7, 8],
  ])));
})();

(function TestPositionalEmpty() {
  assertArrayEquals([], Array.from(Iterator.zip([])));
})();

(function TestPositionalLongest() {
  assertArrayEquals([
    [0, 1],
    [undefined, 2],
  ], Array.from(Iterator.zip([
    [0],
    [1, 2],
  ], { mode: 'longest' })));
})();

(function TestPositionalStrict() {
  let result =
    Iterator.zip([
      [0],
      [1, 2],
    ], { mode: 'strict' });
  assertThrows(() => {
    Array.from(result);
  }, TypeError);
})();

(function TestPositionalPadding() {
  const padding = [{}, {}, {}, {}];

  assertArrayEquals([
    [0, 1],
    [padding[0], 2],
    [padding[0], 3],
  ], Array.from(Iterator.zip([
    [0],
    [1, 2, 3],
  ], { mode: 'longest', padding })));

  assertArrayEquals([
    [0, 1, 4, padding[3]],
    [padding[0], 2, 5, padding[3]],
    [padding[0], 3, padding[2], padding[3]],
  ], Array.from(Iterator.zip([
    [0],
    [1, 2, 3],
    [4, 5],
    [],
  ], { mode: 'longest', padding })));
})();

(function TestNextFirstErrorPropagation() {
  let log = [];
  const iter1 = {
    next() { log.push("n1"); throw new Error("error1"); },
    return() { log.push("r1"); }
  };
  Object.setPrototypeOf(iter1, Iterator.prototype);
  const iter2 = {
    next() { log.push("n2"); throw new Error("error2"); },
    return() { log.push("r2"); }
  };
  Object.setPrototypeOf(iter2, Iterator.prototype);

  const zipped = Iterator.zip([iter1, iter2]);

  try {
    zipped.next();
    assertUnreachable();
  } catch (e) {
    assertEquals("error1", e.message);
  }

  assertEquals(["n1", "r2"], log);
})();

(function TestReturnFirstErrorPropagation() {
  let log = [];
  const iter1 = {
    next() { return { value: 1, done: false }; },
    return() { log.push("r1"); throw new Error("error1"); }
  };
  Object.setPrototypeOf(iter1, Iterator.prototype);
  const iter2 = {
    next() { return { value: 2, done: false }; },
    return() { log.push("r2"); throw new Error("error2"); }
  };
  Object.setPrototypeOf(iter2, Iterator.prototype);

  const zipped = Iterator.zip([iter1, iter2]);
  const result = zipped.next();
  assertArrayEquals(result.value, [1, 2]);
  assertFalse(result.done);
  assertArrayEquals([], log);

  try {
    zipped.return();
    assertUnreachable();
  } catch (e) {
    assertEquals("error2", e.message);
  }

  assertArrayEquals(["r2", "r1"], log);
})();

(function RegressionTestOnOddCapacityNonIterable() {
  const manyIterables = [];
  for (let i = 0; i < 76; i++) {
    manyIterables.push([1]);
  }
  // The 77th iterable. Its push triggers growth to 211 capacity.
  // We make it non-iterable to trigger the catch block.
  manyIterables.push(1);

  assertThrows(() => {
    Iterator.zip(manyIterables);
  }, TypeError);
})();

(function RegressionTestOnOddCapacityNextThrows() {
  const manyIterables = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        next() {
          if (count++ < 76) return { value: [1], done: false };
          throw new Error("fail");
        }
      };
    }
  };
  assertThrows(() => Iterator.zip(manyIterables), Error);
})();

(function RegressionTestOnOddCapacityPaddingInitThrows() {
  const manyIterables = [];
  for (let i = 0; i < 77; i++) manyIterables.push([1]);

  const options = {
    mode: 'longest',
    padding: {
      get [Symbol.iterator]() { throw new Error("fail"); }
    }
  };

  assertThrows(() => Iterator.zip(manyIterables, options), Error);
})();

(function RegressionTestOnOddCapacityPaddingNextThrows() {
  const manyIterables = [];
  for (let i = 0; i < 77; i++) manyIterables.push([1]);

  const padding = {
    [Symbol.iterator]() {
      return {
        next() { throw new Error("fail"); }
      };
    }
  };

  const options = { mode: 'longest', padding };
  assertThrows(() => Iterator.zip(manyIterables, options), Error);
})();

(function RegressionTestOnOddCapacityPaddingCloseThrows() {
  const manyIterables = [];
  for (let i = 0; i < 77; i++) manyIterables.push([1]);

  const padding = {
    [Symbol.iterator]() {
      return {
        next() { return { value: 1, done: false }; },
        return() { throw new Error("fail"); }
      };
    }
  };

  const options = { mode: 'longest', padding };
  assertThrows(() => Iterator.zip(manyIterables, options), Error);
})();
