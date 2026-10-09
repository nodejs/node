// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function noop(a) {
  let r = a + 1;
  r = r + 2;
  r = r + 3;
  r = r + 4;
  r = r + 5;
  r = r + 6;
  r = r + 7;
  return r;
}

function dummy() {}

function outer(trigger) {
  let x = 1;
  function mid() {
    let a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4,
        a5 = 5, a6 = 6, a7 = 7, a8 = 8, a9 = 9,
        a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14;
    return function f() {
      x = 2;
      return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 +
             a8 + a9 + a10 + a11 + a12 + a13 + a14;
    };
  }

  let box = { fn: dummy };
  box.fn = mid();
  x = 1;
  noop(trigger);
  let g = box.fn;
  g();
  return x;
}

%PrepareFunctionForOptimization(noop);
%PrepareFunctionForOptimization(outer);
for (let i = 0; i < 10; i++) {
  assertEquals(2, outer(i));
}
%OptimizeFunctionOnNextCall(outer);
assertEquals(2, outer(10));
