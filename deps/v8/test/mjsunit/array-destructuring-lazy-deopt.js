// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --array-destructure-bytecode

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
