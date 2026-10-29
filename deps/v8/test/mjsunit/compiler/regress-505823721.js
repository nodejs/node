// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev --turbofan

function expectedXor(x, a1, a2, a3, a4, a5, a6, a7, a8, a9) {
  const rot = ((x << 5) ^ (x >>> 27)) | 0;
  return (rot ^ a1 ^ a2 ^ a3 ^ a4 ^ a5 ^ a6 ^ a7 ^ a8 ^ a9) | 0;
}

function victimXor(x, a1, a2, a3, a4, a5, a6, a7, a8, a9) {
  const shl = x << 5;
  const shr = x >>> 27;
  return (((((((((shl ^ a1) ^ a2) ^ a3) ^ a4) ^ a5) ^ a6) ^ a7) ^ a8) ^ a9) ^ shr;
}

function expectedOr(x, a1, a2, a3, a4, a5, a6, a7, a8, a9) {
  const rot = ((x << 5) | (x >>> 27)) | 0;
  return (rot | a1 | a2 | a3 | a4 | a5 | a6 | a7 | a8 | a9) | 0;
}

function victimOr(x, a1, a2, a3, a4, a5, a6, a7, a8, a9) {
  const shl = x << 5;
  const shr = x >>> 27;
  return (((((((((shl | a1) | a2) | a3) | a4) | a5) | a6) | a7) | a8) | a9) | shr;
}

function optimizeAndCheck(fn, expected, args) {
  %PrepareFunctionForOptimization(fn);
  assertEquals(expected(...args), fn(...args));
  %OptimizeFunctionOnNextCall(fn);
  assertEquals(expected(...args), fn(...args));
  assertOptimized(fn);
}

const xorArgs = [
  0x12345678,
  0x11111111,
  0x22222222,
  0x33333333,
  0x44444444,
  0x55555555,
  0x66666666,
  0x77777777,
  0x13579bdf,
  0x2468ace0,
];

const orArgs = [
  0x00000001,
  0x00000000,
  0x00000000,
  0x00000000,
  0x00000000,
  0x00000000,
  0x00000000,
  0x40000000,
  0x10000000,
  0x01000000,
];

optimizeAndCheck(victimXor, expectedXor, xorArgs);
optimizeAndCheck(victimOr, expectedOr, orArgs);
