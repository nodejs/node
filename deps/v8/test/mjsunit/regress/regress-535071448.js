// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --turbolev --array-destructure-bytecode

// Destructuring patterns with more than three targets use
// ArrayDestructureLazyDeoptContinuation as the top deopt frame, which handles
// register writes itself rather than via LazyDeoptInfo result_size.

(function TestFourElements() {
  function f(obj) {
    let [a, b, c, d] = obj;
    return [a, b, c, d];
  }

  %PrepareFunctionForOptimization(f);
  assertArrayEquals([1, 2, 3, 4], f([1, 2, 3, 4]));
  assertArrayEquals([1, 2, 3, 4], f([1, 2, 3, 4]));
  %OptimizeMaglevOnNextCall(f);
  assertArrayEquals([1, 2, 3, 4], f([1, 2, 3, 4]));
})();

(function TestManyElements() {
  function f(obj) {
    let [a, b, c, d, e, g, h, i, j, k] = obj;
    return a + b + c + d + e + g + h + i + j + k;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(55, f([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]));
  assertEquals(55, f([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(55, f([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]));
  assertOptimized(f);
})();

(function TestShortIterable() {
  function f(obj) {
    let [a, b, c, d, e] = obj;
    return [a, b, c, d, e];
  }

  %PrepareFunctionForOptimization(f);
  assertArrayEquals([1, 2, undefined, undefined, undefined], f([1, 2]));
  assertArrayEquals([1, 2, undefined, undefined, undefined], f([1, 2]));
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals([1, 2, undefined, undefined, undefined], f([1, 2]));
  assertOptimized(f);
})();

(function TestLazyDeoptOnFirstNext() {
  function f(obj) {
    let [a, b, c, d] = obj;
    return [a, b, c, d];
  }

  let do_deopt = false;
  let iterable = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        next() {
          count++;
          if (count === 1 && do_deopt) %DeoptimizeFunction(f);
          if (count <= 4) return { value: count * 10, done: false };
          return { value: undefined, done: true };
        }
      };
    }
  };

  %PrepareFunctionForOptimization(f);
  assertArrayEquals([10, 20, 30, 40], f(iterable));
  assertArrayEquals([10, 20, 30, 40], f(iterable));
  %OptimizeMaglevOnNextCall(f);
  assertArrayEquals([10, 20, 30, 40], f(iterable));

  do_deopt = true;
  assertArrayEquals([10, 20, 30, 40], f(iterable));
})();

(function TestLazyDeoptMidIteration() {
  function f(obj) {
    let [a, b, c, d, e] = obj;
    return [a, b, c, d, e];
  }

  let do_deopt = false;
  let iterable = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        next() {
          count++;
          if (count === 3 && do_deopt) %DeoptimizeFunction(f);
          if (count <= 5) return { value: count * 10, done: false };
          return { value: undefined, done: true };
        }
      };
    }
  };

  %PrepareFunctionForOptimization(f);
  assertArrayEquals([10, 20, 30, 40, 50], f(iterable));
  assertArrayEquals([10, 20, 30, 40, 50], f(iterable));
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals([10, 20, 30, 40, 50], f(iterable));
  assertOptimized(f);

  do_deopt = true;
  assertArrayEquals([10, 20, 30, 40, 50], f(iterable));
  assertUnoptimized(f);
})();

(function TestLazyDeoptOnDone() {
  function f(obj) {
    let [a, b, c, d, e] = obj;
    return [a, b, c, d, e];
  }

  let do_deopt = false;
  let iterable = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        next() {
          count++;
          if (count === 1) return { value: 10, done: false };
          if (count === 2 && do_deopt) %DeoptimizeFunction(f);
          return { value: undefined, done: true };
        }
      };
    }
  };

  %PrepareFunctionForOptimization(f);
  assertArrayEquals([10, undefined, undefined, undefined, undefined], f(iterable));
  assertArrayEquals([10, undefined, undefined, undefined, undefined], f(iterable));
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals([10, undefined, undefined, undefined, undefined], f(iterable));
  assertOptimized(f);

  do_deopt = true;
  assertArrayEquals([10, undefined, undefined, undefined, undefined], f(iterable));
  assertUnoptimized(f);
})();

(function TestIteratorGetterDeopt() {
  function f(obj) {
    let [a, b, c, d] = obj;
    return [a, b, c, d];
  }

  let do_deopt = false;
  let iterable = {
    get [Symbol.iterator]() {
      if (do_deopt) %DeoptimizeFunction(f);
      return function() {
        let count = 0;
        return {
          next() {
            count++;
            if (count <= 4) return { value: count, done: false };
            return { value: undefined, done: true };
          }
        };
      };
    }
  };

  %PrepareFunctionForOptimization(f);
  assertArrayEquals([1, 2, 3, 4], f(iterable));
  assertArrayEquals([1, 2, 3, 4], f(iterable));
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals([1, 2, 3, 4], f(iterable));
  assertOptimized(f);

  do_deopt = true;
  assertArrayEquals([1, 2, 3, 4], f(iterable));
  assertUnoptimized(f);
})();

(function TestNextGetterDeopt() {
  function f(obj) {
    let [a, b, c, d] = obj;
    return [a, b, c, d];
  }

  let do_deopt = false;
  let iterable = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        get next() {
          if (do_deopt) %DeoptimizeFunction(f);
          return function() {
            count++;
            if (count <= 4) return { value: count * 100, done: false };
            return { value: undefined, done: true };
          };
        }
      };
    }
  };

  %PrepareFunctionForOptimization(f);
  assertArrayEquals([100, 200, 300, 400], f(iterable));
  assertArrayEquals([100, 200, 300, 400], f(iterable));
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals([100, 200, 300, 400], f(iterable));
  assertOptimized(f);

  do_deopt = true;
  assertArrayEquals([100, 200, 300, 400], f(iterable));
  assertUnoptimized(f);
})();

(function TestIteratorCloseDeopt() {
  function f(obj) {
    let [a, b, c, d] = obj;
    return [a, b, c, d];
  }

  let do_deopt = false;
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
          if (do_deopt) %DeoptimizeFunction(f);
          return {};
        }
      };
    }
  };

  %PrepareFunctionForOptimization(f);
  assertArrayEquals([10, 20, 30, 40], f(iterable));
  assertArrayEquals([10, 20, 30, 40], f(iterable));
  assertEquals(2, return_called);
  return_called = 0;
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals([10, 20, 30, 40], f(iterable));
  assertEquals(1, return_called);
  assertOptimized(f);

  return_called = 0;
  do_deopt = true;
  assertArrayEquals([10, 20, 30, 40], f(iterable));
  assertEquals(1, return_called);
  assertUnoptimized(f);
})();
