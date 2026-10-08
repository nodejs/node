// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan
// Flags: --max-maglev-optimized-bytecode-size=61440

// Keeps `count` Int32 and Float64 values live at once, so Maglev needs roughly
// two stack slots per value while the bytecode stays small.
function buildWideFrame(count, osr) {
  let preload = '';
  let integerFold = '';
  let doubleFold = '';
  for (let i = 0; i < count; ++i) {
    preload += `0.5 + (input ^ ${i});`;
    integerFold += `integer ^= (input ^ ${i});`;
    doubleFold += `double += (input ^ ${i});`;
  }
  return Function(`
    return function wide(input) {
      input |= 0;
      let result = 0;
      for (let iteration = 0; iteration < 20; ++iteration) {
        ${osr ? 'if (iteration === 5) %OptimizeOsr();' : ''}
        ${preload}
        let integer = 0;
        ${integerFold}
        let double = 0.5;
        ${doubleFold}
        result = (integer + double) | 0;
      }
      return result;
    };
  `)();
}

// ~750 stack slots, ~16KB of bytecode.
const kWideValueCount = 400;

const wide = buildWideFrame(kWideValueCount, false);
%PrepareFunctionForOptimization(wide);
const expected = wide(1);
%OptimizeMaglevOnNextCall(wide);
assertEquals(expected, wide(1));
assertFalse(isOptimized(wide));

const wideOsr = buildWideFrame(kWideValueCount, true);
%PrepareFunctionForOptimization(wideOsr);
assertEquals(expected, wideOsr(1));

// One live value, but more bytecode than Maglev is willing to compile.
function buildLongBytecode(count) {
  let body = '';
  for (let i = 0; i < count; ++i) body += `sum += ${i};`;
  return Function(`return function long(sum) { ${body} return sum; };`)();
}

const long = buildLongBytecode(12000);
%PrepareFunctionForOptimization(long);
const expectedLong = long(0);
%OptimizeMaglevOnNextCall(long);
assertEquals(expectedLong, long(0));
assertFalse(isOptimized(long));
