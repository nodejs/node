// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --verify-heap
// Flags: --turbolev --turbolev-escape-analysis --turbofan

// 1. Non-escaping arrow + normal closures capturing a mutable FunctionContext
// across a loop, with eager deopt materializing both JSFunction and Context.
function sumWithElidedClosures(n, off, deopt) {
  let acc = 0;
  const step = (i) => {
    acc = (acc + i) | 0;
  };
  const readWithOffset = function(delta) {
    return acc + delta;
  };
  %AssertEscapeAnalysisElided(step);
  %AssertEscapeAnalysisElided(readWithOffset);

  for (let i = 1; i <= n; i++) {
    step(i);
  }
  const total = readWithOffset(off);
  if (deopt) {
    // Unreached during warmup, so compiled as an eager deopt that materializes
    // `step`, `readWithOffset`, and their shared `FunctionContext`.
    return [total, step, readWithOffset];
  }
  return total;
}

%PrepareFunctionForOptimization(sumWithElidedClosures);
for (let w = 0; w < 20; w++) {
  assertEquals(11, sumWithElidedClosures(4, 1, false));
}

%OptimizeFunctionOnNextCall(sumWithElidedClosures);
assertEquals(56, sumWithElidedClosures(10, 1, false));
assertOptimized(sumWithElidedClosures);

// Trigger eager deopt, materializing both closures and the captured
// FunctionContext; verify heap integrity and closure callability.
const [deoptTotal, materializedStep, materializedRead] =
    sumWithElidedClosures(10, 1, true);
assertUnoptimized(sumWithElidedClosures);
gc();
assertEquals(56, deoptTotal);
assertEquals('object', typeof materializedRead.prototype);
assertEquals(undefined, materializedStep.prototype);
materializedStep(5);
assertEquals(60, materializedRead(0));

// 2. Non-escaping callback closure + captured context in Array.prototype.forEach.
function forEachWithElidedClosure(arr, scale) {
  let sum = 0;
  const cb = (x) => {
    sum = (sum + x * scale) | 0;
  };
  %AssertEscapeAnalysisElided(cb);
  arr.forEach(cb);
  return sum;
}

const inputArr = [1, 2, 3, 4];
%PrepareFunctionForOptimization(forEachWithElidedClosure);
for (let w = 0; w < 20; w++) {
  assertEquals(30, forEachWithElidedClosure(inputArr, 3));
}

%OptimizeFunctionOnNextCall(forEachWithElidedClosure);
assertEquals(50, forEachWithElidedClosure(inputArr, 5));
assertOptimized(forEachWithElidedClosure);
gc();

// 3. Array.prototype.sort comparator closure specialization via InlinedAllocation.
function sortWithInlinedClosure(a, b, c, d) {
  const arr = [a, b, c, d];
  arr.sort((x, y) => x - y);
  return arr[0] * 1000 + arr[3];
}

%PrepareFunctionForOptimization(sortWithInlinedClosure);
for (let w = 0; w < 20; w++) {
  assertEquals(1004, sortWithInlinedClosure(4, 2, 1, 3));
}
%OptimizeFunctionOnNextCall(sortWithInlinedClosure);
assertEquals(1008, sortWithInlinedClosure(8, 3, 6, 1));
assertOptimized(sortWithInlinedClosure);
gc();

// 4. Transitive escape: FunctionContext escapes via a second returned closure
// AFTER an earlier non-escaping closure mutated a `let` slot in a loop.
function mixedEscapingAndLocalClosures(n) {
  let acc = 0;
  const localStep = (i) => {
    acc = (acc + i) | 0;
  };
  for (let i = 1; i <= n; i++) {
    localStep(i);
  }
  return () => acc;
}

%PrepareFunctionForOptimization(mixedEscapingAndLocalClosures);
for (let w = 0; w < 20; w++) {
  assertEquals(10, mixedEscapingAndLocalClosures(4)());
}
%OptimizeFunctionOnNextCall(mixedEscapingAndLocalClosures);
const reader = mixedEscapingAndLocalClosures(6);
assertOptimized(mixedEscapingAndLocalClosures);
gc();
assertEquals(21, reader());
