// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-always-turbofan --turbofan --js-staging
// Flags: --allow-natives-syntax

var buffer = new ArrayBuffer(64);
var dataview = new DataView(buffer, 8, 24);

function readFloat16(offset, littleEndian) {
  return dataview.getFloat16(offset, littleEndian);
}

function writeFloat16(offset, val, littleEndian) {
  dataview.setFloat16(offset, val, littleEndian);
}

function warmup(f, ...args) {
  %PrepareFunctionForOptimization(f);
  f(0, ...args);
  f(1, ...args);
  %OptimizeFunctionOnNextCall(f);
  f(2, ...args);
  f(3, ...args);
}

const cases = [
  { input: 1.1, expected: Math.f16round(1.1) },
  { input: -1.1, expected: Math.f16round(-1.1) },

  // an integer which rounds down under ties-to-even when cast to float16
  { input: 2049, expected: 2048 },
  // an integer which rounds up under ties-to-even when cast to float16
  { input: 2051, expected: 2052 },
  // smallest normal float16
  { input: 0.00006103515625, expected: 0.00006103515625 },
  // largest subnormal float16
  { input: 0.00006097555160522461, expected: 0.00006097555160522461 },
  // smallest float16
  { input: 5.960464477539063e-8, expected: 5.960464477539063e-8 },
  // largest double which rounds to 0 when cast to float16
  { input: 2.9802322387695312e-8, expected: 0 },
  // smallest double which does not round to 0 when cast to float16
  { input: 2.980232238769532e-8, expected: 5.960464477539063e-8 },
  // a double which rounds up to a subnormal under ties-to-even when cast to float16
  { input: 8.940696716308594e-8, expected: 1.1920928955078125e-7 },
  // a double which rounds down to a subnormal under ties-to-even when cast to float16
  { input: 1.4901161193847656e-7, expected: 1.1920928955078125e-7 },
  // the next double above the one on the previous line one
  { input: 1.490116119384766e-7, expected: 1.7881393432617188e-7 },
  // max finite float16
  { input: 65504, expected: 65504 },
  // largest double which does not round to infinity when cast to float16
  { input: 65519.99999999999, expected: 65504 },
  // lowest negative double which does not round to infinity when cast to float16
  { input: -65519.99999999999, expected: -65504 },
  // smallest double which rounds to a non-subnormal when cast to float16
  { input: 0.000061005353927612305, expected: 0.00006103515625 },
  // largest double which rounds to a subnormal when cast to float16
  { input: 0.0000610053539276123, expected: 0.00006097555160522461 },
  { input: NaN, expected: NaN },
  { input: Infinity, expected: Infinity },
  { input: -Infinity, expected: -Infinity },
  // smallest double which rounds to infinity when cast to float16
  { input: 65520, expected: Infinity },
  { input: -65520, expected: - Infinity },

];

warmup(writeFloat16, 1.1);
warmup(readFloat16);

for (let {input, expected} of cases) {
  for (let lilEndian of [true, false]) {
    writeFloat16(0, input, lilEndian);
    assertOptimized(writeFloat16);
    assertEquals(expected, readFloat16(0, lilEndian));
    assertOptimized(writeFloat16);
  }
}
