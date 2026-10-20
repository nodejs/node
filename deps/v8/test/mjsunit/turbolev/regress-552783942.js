// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

const f64 = new Float64Array(1);
const u32 = new Uint32Array(f64.buffer);
u32[0] = 0xFFF7FFFF;
u32[1] = 0xFFF7FFFF;
const holeNan = f64[0];

function dummy(flag) {
  if (flag) {
    let q = 1;
    for (let i = 0; i < 10; ++i) q += i;
  }
  return -0;
}

function makeMin(x, flag) {
  const o = {};
  o.p = Math.min(x, x) + dummy(flag);
  return o;
}

(function testMinIdentityTurbolev() {
  %PrepareFunctionForOptimization(dummy);
  %PrepareFunctionForOptimization(makeMin);
  makeMin(1.5, false);
  makeMin(2.5, false);
  %OptimizeFunctionOnNextCall(makeMin);

  const victim = makeMin(holeNan, false);

  function readP() {
    return victim.p;
  }

  %PrepareFunctionForOptimization(readP);
  readP();
  readP();
  %OptimizeFunctionOnNextCall(readP);

  assertTrue(isNaN(readP()));
  victim.p = 42.5;
  assertEquals(42.5, readP());
})();

function makeMax(x, flag) {
  const o = {};
  o.p = Math.max(x, x) + dummy(flag);
  return o;
}

(function testMaxIdentityTurbolev() {
  %PrepareFunctionForOptimization(dummy);
  %PrepareFunctionForOptimization(makeMax);
  makeMax(1.5, false);
  makeMax(2.5, false);
  %OptimizeFunctionOnNextCall(makeMax);

  const victim = makeMax(holeNan, false);

  function readP() {
    return victim.p;
  }

  %PrepareFunctionForOptimization(readP);
  readP();
  readP();
  %OptimizeFunctionOnNextCall(readP);

  assertTrue(isNaN(readP()));
  victim.p = 84.5;
  assertEquals(84.5, readP());
})();

(function testReturnedBitsQuietedTurbolev() {
  function foldMin(ta) {
    const x = ta[0];
    return Math.min(x, x);
  }
  %PrepareFunctionForOptimization(foldMin);
  foldMin(new Float64Array([1.1]));
  foldMin(new Float64Array([2.2]));
  %OptimizeFunctionOnNextCall(foldMin);

  const r = foldMin(f64);
  const out = new Float64Array(1);
  const outU64 = new BigUint64Array(out.buffer);
  out[0] = r;

  assertTrue(isNaN(r));
  assertNotEquals(0xFFF7FFFF_FFF7FFFFn, outU64[0]);
  const kQuietNanBit = 0x00080000_00000000n;
  assertTrue((outU64[0] & kQuietNanBit) !== 0n);
})();
