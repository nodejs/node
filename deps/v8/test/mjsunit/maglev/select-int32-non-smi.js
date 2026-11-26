// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --turbolev

// Verify that BuildInt32Min / BuildInt32Max / SelectInt32 do not emit
// CheckedSmiSizedInt32 and deopt on non-Smi int32 values (e.g. INT32_MAX).

const s = 'abcdefg';
const kHalfInt32Max = 2 ** 30 - 1;  // x * 2 + 1 == 2147483647 (INT32_MAX)

function substringStartMax(str, x) {
  return str.substring(x * 2 + 1, 2);
}

function substringEndMax(str, x) {
  return str.substring(1, x * 2 + 1);
}

function indexOfStartMax(str, x) {
  return str.indexOf('a', x * 2 + 1);
}

function mathMax3(x, y, z) {
  return Math.max(x * 2 + 1, y | 0, z | 0);
}

function mathMin3(x, y, z) {
  return Math.min(x * -2 - 2, y | 0, z | 0);
}

function atInt32Min(arr, x) {
  return arr.at(x * -2 - 2);
}

// Tagged parameters as inputs.
function substringParams(str, x, y) {
  return str.substring(x, y);
}

// Warm up with Smi inputs so that the arithmetic collects Smi feedback.
for (const [f, warmup_args, args, expected] of [
       [substringStartMax, [s, 1], [s, kHalfInt32Max], 'cdefg'],
       [substringEndMax, [s, 1], [s, kHalfInt32Max], 'bcdefg'],
       [indexOfStartMax, [s, 1], [s, kHalfInt32Max], -1],
       [mathMax3, [1, 1, 0], [kHalfInt32Max, 0, 1], 2147483647],
       [mathMin3, [1, 1, 0], [kHalfInt32Max, 0, -1], -2147483648],
       [
         atInt32Min, [[1, 2, 3, 4, 5], 1], [[1, 2, 3, 4, 5], kHalfInt32Max],
         undefined
       ],
       [substringParams, [s, 1, 0], [s, 2, 5], 'cde'],
     ]) {
  %PrepareFunctionForOptimization(f);
  for (let i = 0; i < 10; i++) f(...warmup_args);
  %OptimizeFunctionOnNextCall(f);
  f(...warmup_args);
  assertOptimized(f);
  assertEquals(expected, f(...args));
  assertOptimized(f);
}
