// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-iterator-chunking

function* gen(n) {
  for (let i = 1; i <= n; ++i) yield i;
}

(function TestChunksBasic() {
  assertArrayEquals([[1, 2], [3, 4], [5]], Array.from(gen(5).chunks(2)));
})();

(function TestChunksExactMultiple() {
  assertArrayEquals([[1, 2, 3, 4, 5]], Array.from(gen(5).chunks(5)));
})();

(function TestChunksPartial() {
  assertArrayEquals([[1, 2, 3]], Array.from(gen(3).chunks(5)));
})();

(function TestWindowsBasic() {
  assertArrayEquals(
      [[1, 2, 3], [2, 3, 4], [3, 4, 5]], Array.from(gen(5).windows(3)));
})();

(function TestWindowsPartialDisallowed() {
  assertArrayEquals([], Array.from(gen(3).windows(5, 'only-full')));
})();

(function TestWindowsPartialAllowed() {
  assertArrayEquals(
      [[1, 2, 3]], Array.from(gen(3).windows(5, 'allow-partial')));
})();

(function RegressionTestWindowsAllowPartialNoTrailing() {
  const iter = gen(5).windows(3, 'allow-partial');
  assertEquals({value: [1, 2, 3], done: false}, iter.next());
  assertEquals({value: [2, 3, 4], done: false}, iter.next());
  assertEquals({value: [3, 4, 5], done: false}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
})();

// Tests that the "undersized" argument strictly rejects non-string values
// (including objects with custom toString, wrapper objects) without performing
// any string coercion.
(function RegressionTestWindowsUndersizedNoCoercion() {
  assertThrows(() => gen(5).windows(2, new String('only-full')), TypeError);
  const onlyFullObj = {
    toString() {
      return 'only-full';
    }
  };
  assertThrows(() => gen(5).windows(2, onlyFullObj), TypeError);
  const allowPartialObj = {
    toString() {
      return 'allow-partial';
    }
  };
  assertThrows(() => gen(5).windows(2, allowPartialObj), TypeError);
})();

(function TestLargeChunkAndWindowSizes() {
  const expected = Array.from(gen(20));
  for (const size of [2 ** 30, 2 ** 31, 2 ** 32 - 1]) {
    assertArrayEquals([expected], Array.from(gen(20).chunks(size)));
    assertArrayEquals([], Array.from(gen(20).windows(size, 'only-full')));
    assertArrayEquals(
        [expected], Array.from(gen(20).windows(size, 'allow-partial')));
  }
})();

(function TestWindowsCircularBufferWrapAround() {
  const n = 2500;
  const w = 1025;
  let i = 0;
  for (const win of gen(n).windows(w)) {
    assertEquals(w, win.length);
    assertEquals(i + 1, win[0]);
    assertEquals(i + Math.floor(w / 2) + 1, win[Math.floor(w / 2)]);
    assertEquals(i + w, win[w - 1]);
    ++i;
  }
  assertEquals(n - w + 1, i);
})();

(function TestWindowsDoubleCapacityGrowAndCopyBack() {
  for (const w of [5, 10]) {
    const n = 50;
    let i = 0;
    for (const win of gen(n).windows(w)) {
      assertArrayEquals(Array.from({length: w}, (_, j) => i + j + 1), win);
      ++i;
    }
    assertEquals(n - w + 1, i);
  }
})();

(function TestWindowsMidIterationReturn() {
  let returnCalled = 0;
  let step = 0;
  const underlyingWithReturn = {
    [Symbol.iterator]() {
      return this;
    },
    next() {
      return {value: ++step, done: false};
    },
    return() {
      ++returnCalled;
      return {value: undefined, done: true};
    }
  };
  const iter = Iterator.from(underlyingWithReturn).windows(3);
  assertEquals({value: [1, 2, 3], done: false}, iter.next());
  assertEquals(0, returnCalled);
  assertEquals({value: undefined, done: true}, iter.return());
  assertEquals(1, returnCalled);
  assertEquals({value: undefined, done: true}, iter.next());
  assertEquals(3, step);
})();

(function TestWindowsMidIterationThrow() {
  let count = 0;
  const underlyingThrows = {
    [Symbol.iterator]() {
      return this;
    },
    next() {
      if (++count === 5) throw new Error('boom');
      return {value: count, done: false};
    }
  };
  const iter = Iterator.from(underlyingThrows).windows(3);
  assertEquals({value: [1, 2, 3], done: false}, iter.next());
  assertEquals({value: [2, 3, 4], done: false}, iter.next());
  assertThrows(() => iter.next(), Error, 'boom');
  assertEquals({value: undefined, done: true}, iter.next());
})();
