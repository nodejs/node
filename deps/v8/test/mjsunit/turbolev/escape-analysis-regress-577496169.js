// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

function makeClosure(x) {
  let a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8;
  let capturedOuter = x + a + b + c + d + e + f + g + h;
  return function inner(n, arg, fn, keepGoing) {
    let innerVar = capturedOuter;
    function captureInner() { return innerVar; }
    try {
      let k = n & 3;
      for (let i = 0; i < 3; i++) {
        let r = fn(arg);
        if (!keepGoing && k < 10) {
          return [r, captureInner];
        }
        fn = captureInner;
      }
    } catch (e) {
      return e;
    }
  };
}

function caller(n, arg) {
  let fn = makeClosure(n);
  return [fn(n, arg, Math.floor, false), fn];
}

const warm = makeClosure(1);
%PrepareFunctionForOptimization(makeClosure);
%PrepareFunctionForOptimization(warm);
%PrepareFunctionForOptimization(caller);

warm(1, 1.5, x => x, true);
warm(1, 1.5, Math.ceil, true);
caller(1, 1.5);
caller(2, "2.5");

%OptimizeFunctionOnNextCall(caller);
assertEquals(2, caller(1, "2.5")[0][0]);
