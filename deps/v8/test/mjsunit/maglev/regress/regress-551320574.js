// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-non-eager-inlining

const f64 = new Float64Array(1);
const u32 = new Uint32Array(f64.buffer);
u32[0] = 0xFFF7FFFF;
u32[1] = 0xFFF7FFFF;
const holeNan = f64[0];

function minusZero(flag) {
  if (flag) {
    let q = 1;
    for (let i = 0; i < 10; ++i) q += i;
  }
  return -0;
}

function makeAdd(x, flag) {
  const o = {};
  o.p = x + minusZero(flag);
  return o;
}

(function testAddIdentity() {
  %PrepareFunctionForOptimization(minusZero);
  %PrepareFunctionForOptimization(makeAdd);
  makeAdd(1.5, false);
  makeAdd(2.5, false);
  %OptimizeMaglevOnNextCall(makeAdd);

  const victim = makeAdd(holeNan, false);

  function readP() {
    return victim.p;
  }

  %PrepareFunctionForOptimization(readP);
  readP();
  readP();
  %OptimizeMaglevOnNextCall(readP);

  assertTrue(isNaN(readP()));
  victim.p = 42.5;
  assertEquals(42.5, readP());
})();

function one(flag) {
  if (flag) {
    let q = 1;
    for (let i = 0; i < 10; ++i) q += i;
  }
  return 1.0;
}

function makeMul(x, flag) {
  const o = {};
  o.p = x * one(flag);
  return o;
}

(function testMulIdentity() {
  %PrepareFunctionForOptimization(one);
  %PrepareFunctionForOptimization(makeMul);
  makeMul(1.5, false);
  makeMul(2.5, false);
  %OptimizeMaglevOnNextCall(makeMul);

  const victim = makeMul(holeNan, false);

  function readP() {
    return victim.p;
  }

  %PrepareFunctionForOptimization(readP);
  readP();
  readP();
  %OptimizeMaglevOnNextCall(readP);

  assertTrue(isNaN(readP()));
  victim.p = 84.5;
  assertEquals(84.5, readP());
})();
