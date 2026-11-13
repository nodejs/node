// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// DataView.prototype.setFloat64/setFloat32 must write the same bytes for a
// signaling NaN before and after optimization.

const source = new ArrayBuffer(8);
new BigUint64Array(source)[0] = 0xfff2800000000000n;
const signalingNaN = new Float64Array(source)[0];

const target = new ArrayBuffer(8);
const dv = new DataView(target);

function storeFloat64(value) {
  dv.setFloat64(0, value, true);
}
function storeFloat64BigEndian(value) {
  dv.setFloat64(0, value, false);
}
function storeFloat32(value) {
  dv.setFloat32(0, value, true);
}
function storeFloat64DynamicEndian(value, littleEndian) {
  dv.setFloat64(0, value, littleEndian);
}

function run(f, ...args) {
  dv.setBigUint64(0, 0n);
  f(signalingNaN, ...args);
  return dv.getBigUint64(0, true);
}

function test(f, ...args) {
  %PrepareFunctionForOptimization(f);
  const expected = run(f, ...args);
  %OptimizeMaglevOnNextCall(f);
  assertEquals(expected, run(f, ...args));
  assertOptimized(f);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected, run(f, ...args));
  assertOptimized(f);
}

test(storeFloat64);
test(storeFloat64BigEndian);
test(storeFloat32);
test(storeFloat64DynamicEndian, true);
