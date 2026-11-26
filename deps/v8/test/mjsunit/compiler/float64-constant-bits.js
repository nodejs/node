// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// Float64 constants in optimized code keep their exact bits, both the ones a
// single instruction can build on some targets (riscv64's Zfa fli.d) and
// their neighbours that it cannot.
const kValues = [
  1, -1, 0.5, 2, 1.25, 3, 16, 65536, Infinity, NaN,
  2 ** -16, 2 ** -1022,  // fli.d entries 2 and 1
  2 ** -126,             // fli.s entry 1, but not an fli.d entry
  0.1, -0.5, -2, -Infinity, 0, -0, 1e300,
];

function literal(v) {
  return Object.is(v, -0) ? '-0' : String(v);
}

const store = new Function(
    'a', kValues.map((v, i) => `a[${i}] = ${literal(v)};`).join('\n'));

function bits(f64) {
  return Array.from(new Uint32Array(f64.buffer));
}

const expected = new Float64Array(kValues.length);
store(expected);

%PrepareFunctionForOptimization(store);
store(new Float64Array(kValues.length));
%OptimizeFunctionOnNextCall(store);
const actual = new Float64Array(kValues.length);
store(actual);
assertOptimized(store);
assertEquals(bits(expected), bits(actual));

function arith(x) {
  return (x + 1) * 0.5 - 2 / x - (x < 0 ? Infinity : -1);
}
const inputs = [3, -7.5, 1e-300];
const arithExpected = inputs.map(arith);
%PrepareFunctionForOptimization(arith);
inputs.forEach(arith);
%OptimizeFunctionOnNextCall(arith);
assertEquals(arithExpected, inputs.map(arith));
assertOptimized(arith);
