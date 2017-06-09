// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function mul(a, b) {
  const l = a & 0x3ffffff;
  const h = b & 0x3ffffff;

  return (l * h) >>> 0;
}

function mulAndDiv(a, b) {
  const l = a & 0x3ffffff;
  const h = b & 0x3ffffff;
  const m = l * h;

  const rl = m & 0x3ffffff;
  const rh = (m / 0x4000000) >>> 0;

  return rl | rh;
}

function overflowMul(a, b) {
  const l = a | 0;
  const h = b | 0;

  return (l * h) >>> 0;
}

function overflowDiv(a, b) {
  const l = a & 0x3ffffff;
  const h = b & 0x3ffffff;
  const m = l * h;

  return (m / 0x10) >>> 0;
}

function nonPowerOfTwoDiv(a, b) {
  const l = a & 0x3ffffff;
  const h = b & 0x3ffffff;
  const m = l * h;

  return (m / 0x4000001) >>> 0;
}

function test(fn, a, b, sets) {
  const expected = fn(a, b);
  fn(1, 2);
  fn(0, 0);
  %OptimizeFunctionOnNextCall(fn);
  const actual = fn(a, b);

  assertEquals(expected, actual);

  sets.forEach(function(set, i) {
    assertEquals(set.expected, fn(set.a, set.b), fn.name + ', set #' + i);
  });
}

test(mul, 0x3ffffff, 0x3ffffff, [
  { a: 0, b: 0, expected: 0 },
  { a: 0xdead, b: 0xbeef, expected: 0xa6144983 },
  { a: 0x1aa1dea, b: 0x2badead, expected: 0x35eb2322 }
]);
test(mulAndDiv, 0x3ffffff, 0x3ffffff, [
  { a: 0, b: 0, expected: 0 },
  { a: 0xdead, b: 0xbeef, expected: 0x21449ab },
  { a: 0x1aa1dea, b: 0x2badead, expected: 0x1ebf32f }
]);
test(overflowMul, 0x4ffffff, 0x4ffffff, [
  { a: 0, b: 0, expected: 0 },
  { a: 0xdead, b: 0xbeef, expected: 0xa6144983 },
  { a: 0x1aa1dea, b: 0x2badead, expected: 0x35eb2322 }
]);
test(overflowDiv, 0x3ffffff, 0x3ffffff, [
  { a: 0, b: 0, expected: 0 },
  { a: 0xdead, b: 0xbeef, expected: 0xa614498 },
  { a: 0x1aa1dea, b: 0x2badead, expected: 0x835eb232 }
]);
test(nonPowerOfTwoDiv, 0x3ffffff, 0x3ffffff, [
  { a: 0, b: 0, expected: 0 },
  { a: 0xdead, b: 0xbeef, expected: 0x29 },
  { a: 0x1aa1dea, b: 0x2badead, expected: 0x122d20d }
]);
