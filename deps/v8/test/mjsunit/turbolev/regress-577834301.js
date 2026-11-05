// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function testBranch(cond) {
  while (x && cond) {
    do {
      y;
      y = 1;
    } while (cond);
    let y;
    x = 1;
  }
  if (x) return 1;
  return 0;
  let x;
}

%PrepareFunctionForOptimization(testBranch);
assertThrows(() => testBranch(true), ReferenceError);
%OptimizeFunctionOnNextCall(testBranch);
assertThrows(() => testBranch(true), ReferenceError);

function testToBoolean(cond) {
  while (x && cond) {
    do {
      y;
      y = 1;
    } while (cond);
    let y;
    x = 1;
  }
  return !x;
  let x;
}

%PrepareFunctionForOptimization(testToBoolean);
assertThrows(() => testToBoolean(true), ReferenceError);
%OptimizeFunctionOnNextCall(testToBoolean);
assertThrows(() => testToBoolean(true), ReferenceError);

async function testAsyncBranch() {
  const obj = {};
  async function f(cond) {
    if (cond) {
      for (; (x, cond); ) {
        await 1;
        for (let k = 1; k; k = 0) {
          obj.prop;
        }
      }
      if (x) return 1;
    }
    let x = 1;
    return x;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(1, await f(0));
  assertEquals(1, await f(0));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(1, await f(0));
  await assertThrowsAsync(f(1), ReferenceError);
}
assertPromiseResult(testAsyncBranch());
