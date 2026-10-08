// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function testHoley(arr, arr2) {
  let zero = -0.0;
  for (let i = 0; i < arr.length; i++) {
    let x = arr[i];
    let y = x + zero;
    arr2[i] = y;
    zero = -0.0;
  }
}

function testPacked(arr, arr2) {
  let zero = -0.0;
  for (let i = 0; i < arr.length; i++) {
    let x = arr[i];
    let y = x + zero;
    arr2[i] = y;
    zero = -0.0;
  }
}

function run(test, dst) {
  const src = [, 1.1, 3.3];
  %PrepareFunctionForOptimization(test);
  dst[0] = 5.5;
  test(src, dst);
  assertEquals(NaN, dst[0]);
  assertTrue(0 in dst);

  dst[0] = 5.5;
  test(src, dst);
  dst[0] = 5.5;
  %OptimizeFunctionOnNextCall(test);
  test(src, dst);
  assertEquals(NaN, dst[0]);
  assertTrue(0 in dst);
}

run(testHoley, [, 1.1, 3.3]);
run(testPacked, [1.1, 2.2, 3.3]);
