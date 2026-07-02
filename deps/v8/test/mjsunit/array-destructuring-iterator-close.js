// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --array-destructure-bytecode

(function TestEarlyReturnCalled() {
  let return_called = 0;
  let iterable = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        next() {
          count++;
          return { value: count * 10, done: false };
        },
        return() {
          return_called++;
          return { done: true };
        }
      };
    }
  };

  function f(obj) {
    let [x, y] = obj;
    return x + y;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(30, f(iterable));
  assertEquals(1, return_called);
  assertEquals(30, f(iterable));
  assertEquals(2, return_called);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(30, f(iterable));
  assertEquals(3, return_called);
  assertOptimized(f);
})();

(function TestReturnNotCalledWhenDone() {
  let return_called = 0;
  let iterable = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        next() {
          if (count < 2) {
            count++;
            return { value: count * 10, done: false };
          }
          return { value: undefined, done: true };
        },
        return() {
          return_called++;
          return { done: true };
        }
      };
    }
  };

  function f(obj) {
    let [x, y, z] = obj;
    return x + y;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(30, f(iterable));
  assertEquals(0, return_called);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(30, f(iterable));
  assertEquals(0, return_called);
})();

(function TestReturnThrowingException() {
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() {
          return { value: 42, done: false };
        },
        return() {
          throw new Error("Exception in return");
        }
      };
    }
  };

  function f(obj) {
    let [x] = obj;
    return x;
  }

  %PrepareFunctionForOptimization(f);
  assertThrows(() => f(iterable), Error, "Exception in return");
  %OptimizeFunctionOnNextCall(f);
  assertThrows(() => f(iterable), Error, "Exception in return");
})();

(function TestEmptyPatternCountZero() {
  let iterator_called = 0;
  let return_called = 0;
  let next_called = 0;
  let iterable = {
    [Symbol.iterator]() {
      iterator_called++;
      return {
        next() {
          next_called++;
          return { value: 10, done: false };
        },
        return() {
          return_called++;
          return { done: true };
        }
      };
    }
  };

  function f(obj) {
    let [] = obj;
    return 42;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(42, f(iterable));
  assertEquals(1, iterator_called);
  assertEquals(0, next_called);
  assertEquals(1, return_called);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(42, f(iterable));
  assertEquals(2, iterator_called);
  assertEquals(0, next_called);
  assertEquals(2, return_called);
  assertOptimized(f);
})();

(function TestIteratorNotExhausted() {
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  const iter = map.values();
  const [first] = iter;
  assertEquals('a', first);
  assertEquals({ value: 'b', done: false }, iter.next());
  assertEquals({ value: 'c', done: false }, iter.next());
  assertEquals({ value: undefined, done: true }, iter.next());
})();
