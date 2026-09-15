// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

const MAX = %StringMaxLength();

function makeRope(length) {
  const powers = ['A'];
  while (powers[powers.length - 1].length <= Math.floor(length / 2)) {
    const previous = powers[powers.length - 1];
    powers.push(previous + previous);
  }
  let remaining = length;
  let result = '';
  for (let i = powers.length - 1; i >= 0; --i) {
    const part = powers[i];
    if (part.length <= remaining) {
      result = result === '' ? part : result + part;
      remaining -= part.length;
    }
  }
  assertEquals(length, result.length);
  return result;
}

function target(prefix) {
  const a = prefix + 'A';
  const sum = a.length + 13;
  if (sum < 0) return -3;
  const s = a + 'BBBBBBBBBBBBB';
  if (sum >= MAX) return -1;
  return s.length === sum ? 0 : -2;
}

%PrepareFunctionForOptimization(target);
for (let i = 0; i < 20; ++i) assertEquals(0, target('x'));
%OptimizeFunctionOnNextCall(target);
assertEquals(0, target('x'));

assertEquals(-1, target(makeRope(MAX - 14)));
