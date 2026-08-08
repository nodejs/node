// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --superspreading

function runNearStackLimit(f) {
  let count = -1;
  function t() {
    try {
      t();
      if (count > 0) {
        count--;
      }
      if (count == 1) {
        try {
          f();
        } catch (e) {
          if (e instanceof RangeError) {
            // TODO(olivf): Remove once all platforms support it
            console.log("Warning: Vararg optimization seems missing");
          } else {
            throw e;
          }
        }
      }
      return;
    } catch (e) {
      if (count == -1) {
        // Back off
        count = 1200;
      } else {
        throw e;
      }
    }
  }
  t();
}

const kSize = 32768;
const spread = new Array(kSize);
spread[0] = 42;
spread[1] = 42;
spread[kSize-2] = 42;
spread[kSize-1] = 42;

runNearStackLimit(function TestPushWithSpreadAndArgs() {
  const arr = [];
  arr.push(123, ...spread);
  assertEquals(kSize + 1, arr.length);
  assertEquals(42, arr[1]);
  assertEquals(123, arr[0]);
  assertEquals(42, arr[kSize]);
});

runNearStackLimit(function TestPushWithMultipleArgsAndSpread() {
  const arr = [];
  arr.push(1, 2, 3, ...spread);
  assertEquals(kSize + 3, arr.length);
  assertEquals(1, arr[0]);
  assertEquals(2, arr[1]);
  assertEquals(3, arr[2]);
  assertEquals(42, arr[3]);
  assertEquals(42, arr[kSize + 2]);
});

runNearStackLimit(function TestPushWithSpreadOnly() {
  const arr = [];
  arr.push(...spread);
  assertEquals(kSize, arr.length);
  assertEquals(42, arr[0]);
  assertEquals(undefined, arr[3]);
});

runNearStackLimit(function TestPushWithSpreadAndFollowingArg() {
  const arr = [];
  arr.push(...spread, 1, 2);
  assertEquals(kSize + 2, arr.length);
  assertEquals(42, arr[0]);
  assertEquals(42, arr[kSize-1]);
  assertEquals(1, arr[kSize]);
  assertEquals(2, arr[kSize+1]);
});

runNearStackLimit(function TestPushWithPrecedingArgsAndSpreadAndFollowingArg() {
  const arr = [];
  arr.push(1, 2, ...spread, 11, 12);
  assertEquals(kSize + 4, arr.length);
  assertEquals(1, arr[0]);
  assertEquals(2, arr[1]);
  assertEquals(42, arr[2]);
  assertEquals(42, arr[kSize+1]);
  assertEquals(11, arr[kSize+2]);
  assertEquals(12, arr[kSize+3]);
});

runNearStackLimit(function TestPushWithDoubleSpreadAndArgs() {
  const arr = [];
  arr.push(1, 2, ...spread, 11, 12, ...spread, 333);
  assertEquals(2*kSize + 5, arr.length);
  assertEquals(1, arr[0]);
  assertEquals(2, arr[1]);
  assertEquals(42, arr[2]);
  assertEquals(42, arr[kSize+1]);
  assertEquals(11, arr[kSize+2]);
  assertEquals(12, arr[kSize+3]);
  assertEquals(42, arr[kSize+4]);
  assertEquals(42, arr[2*kSize + 3]);
  assertEquals(333, arr[2*kSize + 4]);
});

runNearStackLimit(function TestPushWithSpreadAndHoles() {
  const arr = [];
  const arr2 = [,,23];
  spread.__proto__ = arr2;
  arr.push(...spread);
  assertEquals(42, arr[0]);
  assertEquals(23, arr[2]);
});

runNearStackLimit(function TestPushWithLargePrecedingArgs() {
  const arr = [];
  const preceding = new Array(100).fill(1);
  arr.push(...preceding, ...spread);
  assertEquals(100 + kSize, arr.length);
});
