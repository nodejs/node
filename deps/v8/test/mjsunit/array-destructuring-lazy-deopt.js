// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --turbolev --array-destructure-bytecode

function f(obj) {
  let [x, y] = obj;
  return x + y;
}

let do_deopt = false;
let iterable = {
  [Symbol.iterator]() {
    let count = 0;
    return {
      next() {
        if (count === 0) {
          if (do_deopt) %DeoptimizeFunction(f);
          count++;
          return { value: 10, done: false };
        } else if (count === 1) {
          count++;
          return { value: 20, done: false };
        }
        return { value: undefined, done: true };
      }
    };
  }
};

%PrepareFunctionForOptimization(f);
assertEquals(30, f(iterable));
assertEquals(30, f(iterable));
%OptimizeFunctionOnNextCall(f);
assertEquals(30, f(iterable));
assertOptimized(f);

do_deopt = true;
assertEquals(30, f(iterable));
assertUnoptimized(f);

(function TestNextGetterDeopt() {
  function g(obj) {
    let [x, y] = obj;
    return x + y;
  }

  let do_deopt = false;
  let next_getter_calls = 0;
  let iterable = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        get next() {
          next_getter_calls++;
          if (do_deopt) %DeoptimizeFunction(g);
          return function() {
            if (count++ === 0) return { value: 100, done: false };
            return { value: 200, done: false };
          };
        }
      };
    }
  };

  %PrepareFunctionForOptimization(g);
  assertEquals(300, g(iterable));
  assertEquals(300, g(iterable));
  %OptimizeFunctionOnNextCall(g);
  assertEquals(300, g(iterable));
  assertOptimized(g);

  next_getter_calls = 0;
  do_deopt = true;
  assertEquals(300, g(iterable));
  assertEquals(1, next_getter_calls);
  assertUnoptimized(g);
})();

(function TestEmptyDestructuringNextGetterBeforeClose() {
  function h(obj) {
    let [] = obj;
    return 42;
  }

  let log = [];
  let iterable = {
    [Symbol.iterator]() {
      return {
        get next() {
          log.push("get next");
          return function() {
            return { value: 1, done: false };
          };
        },
        return() {
          log.push("return");
          return {};
        }
      };
    }
  };

  %PrepareFunctionForOptimization(h);
  assertEquals(42, h(iterable));
  assertEquals(["get next", "return"], log);
  log = [];
  %OptimizeFunctionOnNextCall(h);
  assertEquals(42, h(iterable));
  assertEquals(["get next", "return"], log);
  assertOptimized(h);
})();

(function TestEarlyBreakDeopt() {
  function f(obj) {
    let [x] = obj;
    return x;
  }

  let do_deopt = false;
  let return_called = 0;
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() {
          if (do_deopt) %DeoptimizeFunction(f);
          return { value: 42, done: false };
        },
        return() {
          return_called++;
          return {};
        }
      };
    }
  };

  %PrepareFunctionForOptimization(f);
  assertEquals(42, f(iterable));
  assertEquals(1, return_called);
  return_called = 0;
  %OptimizeFunctionOnNextCall(f);
  assertEquals(42, f(iterable));
  assertEquals(1, return_called);
  assertOptimized(f);

  return_called = 0;
  do_deopt = true;
  assertEquals(42, f(iterable));
  assertEquals(1, return_called);
  assertUnoptimized(f);
})();

(function TestIteratorCloseDeopt() {
  function f(obj) {
    let [x, y] = obj;
    return x + y;
  }

  let do_deopt = false;
  let return_called = 0;
  let iterable = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        next() {
          if (count++ === 0) return { value: 10, done: false };
          return { value: 20, done: false };
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
  assertEquals(30, f(iterable));
  assertEquals(1, return_called);
  return_called = 0;
  %OptimizeFunctionOnNextCall(f);
  assertEquals(30, f(iterable));
  assertEquals(1, return_called);
  assertOptimized(f);

  return_called = 0;
  do_deopt = true;
  assertEquals(30, f(iterable));
  assertEquals(1, return_called);
  assertUnoptimized(f);
})();

(function TestIteratorGetterDeopt() {
  function f(obj) {
    let [x, y] = obj;
    return x + y;
  }

  let do_deopt = false;
  let iterable = {
    get [Symbol.iterator]() {
      if (do_deopt) %DeoptimizeFunction(f);
      return function() {
        let count = 0;
        return {
          next() {
            if (count === 0) {
              count++;
              return { value: 10, done: false };
            } else if (count === 1) {
              count++;
              return { value: 20, done: false };
            }
            return { value: undefined, done: true };
          }
        };
      };
    }
  };

  %PrepareFunctionForOptimization(f);
  assertEquals(30, f(iterable));
  assertEquals(30, f(iterable));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(30, f(iterable));
  assertOptimized(f);

  do_deopt = true;
  assertEquals(30, f(iterable));
  assertUnoptimized(f);
})();

(function TestDoneDeopt() {
  function f(obj) {
    let [x, y, z] = obj;
    return [x, y, z];
  }

  let do_deopt = false;
  let iterable = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        next() {
          if (count === 0) {
            count++;
            return { value: 10, done: false };
          } else if (count === 1) {
            count++;
            if (do_deopt) %DeoptimizeFunction(f);
            return { value: undefined, done: true };
          }
          return { value: undefined, done: true };
        }
      };
    }
  };

  %PrepareFunctionForOptimization(f);
  assertArrayEquals([10, undefined, undefined], f(iterable));
  assertArrayEquals([10, undefined, undefined], f(iterable));
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals([10, undefined, undefined], f(iterable));
  assertOptimized(f);

  do_deopt = true;
  assertArrayEquals([10, undefined, undefined], f(iterable));
  assertUnoptimized(f);
})();

(function TestValueDeopt() {
  function f(obj) {
    let [x, y, z] = obj;
    return [x, y, z];
  }

  let do_deopt = false;
  let iterable = {
    [Symbol.iterator]() {
      let count = 0;
      return {
        next() {
          if (count === 0) {
            count++;
            return { value: 10, done: false };
          } else if (count === 1) {
            count++;
            if (do_deopt) %DeoptimizeFunction(f);
            return { value: 20, done: false };
          } else if (count === 2) {
            count++;
            return { value: 30, done: false };
          }
          return { value: undefined, done: true };
        }
      };
    }
  };

  %PrepareFunctionForOptimization(f);
  assertArrayEquals([10, 20, 30], f(iterable));
  assertArrayEquals([10, 20, 30], f(iterable));
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals([10, 20, 30], f(iterable));
  assertOptimized(f);

  do_deopt = true;
  assertArrayEquals([10, 20, 30], f(iterable));
  assertUnoptimized(f);
})();
