// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function sumWithSiblingPropertyWriteAndTypedArray(state, ta) {
  let sum = 0;
  for (let i = 0; i < state.items.length; ++i) {
    const v = state.items[i];
    ta[i & 3] = v;
    state.count = (state.count + v) | 0;
    sum += state.items[i] + state.stride;
  }
  return sum;
}

const ta = new Int32Array(4);
const state = {
  items: [1, 2, 3, 4],
  stride: 10,
  count: 0,
};

%PrepareFunctionForOptimization(sumWithSiblingPropertyWriteAndTypedArray);
for (let i = 0; i < 5; ++i) {
  state.count = 0;
  assertEquals(50, sumWithSiblingPropertyWriteAndTypedArray(state, ta));
  assertEquals(10, state.count);
  assertEquals(4, ta[3]);
}

%OptimizeMaglevOnNextCall(sumWithSiblingPropertyWriteAndTypedArray);
state.count = 0;
assertEquals(50, sumWithSiblingPropertyWriteAndTypedArray(state, ta));
assertEquals(10, state.count);
assertEquals(4, ta[3]);
assertMaglevved(sumWithSiblingPropertyWriteAndTypedArray);

// Negative test 1: aliased objects (o1 === o2) writing o1.a inside a loop must
// invalidate o2.a while preserving o2.b.
function aliasedPropertyWrite(o1, o2, n) {
  let sumA = o2.a;
  let sumB = o2.b;
  for (let i = 0; i < n; ++i) {
    o1.a = o1.a + 1;
    sumA += o2.a;
    sumB += o2.b;
  }
  return sumA * 1000 + sumB;
}

const shared = { a: 0, b: 7 };
%PrepareFunctionForOptimization(aliasedPropertyWrite);
for (let i = 0; i < 5; ++i) {
  shared.a = 0;
  // i=0: a=1, sumA=1; i=1: a=2, sumA=3; i=2: a=3, sumA=6; sumB = 7 + 3*7 = 28.
  assertEquals(6028, aliasedPropertyWrite(shared, shared, 3));
}
%OptimizeMaglevOnNextCall(aliasedPropertyWrite);
shared.a = 0;
assertEquals(6028, aliasedPropertyWrite(shared, shared, 3));
assertMaglevved(aliasedPropertyWrite);

// Negative test 2: aliased double-field property write inside a loop must
// invalidate cached loads of that double property on aliases.
function aliasedDoublePropertyWrite(o1, o2, n) {
  let sumX = o2.x;
  let sumY = o2.y;
  for (let i = 0; i < n; ++i) {
    o1.x = o1.x + 1.5;
    sumX += o2.x;
    sumY += o2.y;
  }
  return sumX + sumY;
}

const sharedDouble = { x: 0.5, y: 2.25 };
sharedDouble.x = 1.25;
%PrepareFunctionForOptimization(aliasedDoublePropertyWrite);
for (let i = 0; i < 5; ++i) {
  sharedDouble.x = 1.0;
  // Pre-loop: sumX=1.0, sumY=2.25
  // i=0: x=2.5, sumX=3.5, sumY=4.5
  // i=1: x=4.0, sumX=7.5, sumY=6.75 -> total 14.25
  assertEquals(14.25, aliasedDoublePropertyWrite(sharedDouble, sharedDouble, 2));
}
%OptimizeMaglevOnNextCall(aliasedDoublePropertyWrite);
sharedDouble.x = 1.0;
assertEquals(14.25, aliasedDoublePropertyWrite(sharedDouble, sharedDouble, 2));
assertMaglevved(aliasedDoublePropertyWrite);

// Negative test 3: Array.prototype.push inside a loop must invalidate cached
// array length and elements backing store across iterations.
function pushInLoop(arr, n) {
  let lenSum = arr.length;
  let firstSum = arr[0];
  for (let i = 0; i < n; ++i) {
    arr.push(i + 10);
    lenSum += arr.length;
    firstSum += arr[0];
  }
  return lenSum * 100 + firstSum;
}

%PrepareFunctionForOptimization(pushInLoop);
for (let i = 0; i < 5; ++i) {
  const a = [5];
  // lenSum = 1 + 2 + 3 + 4 = 10; firstSum = 5 + 5 + 5 + 5 = 20 -> 1020
  assertEquals(1020, pushInLoop(a, 3));
}
%OptimizeMaglevOnNextCall(pushInLoop);
assertEquals(1020, pushInLoop([5], 3));
assertMaglevved(pushInLoop);
