// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --turbolev --turbofan
// Flags: --maglev-non-eager-inlining
// Flags: --max-maglev-eager-inlined-bytecode-size=0
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

const smiBox = { a: 5, b: -1 };
smiBox.a = 7;

function helperReturnBoolean(x) {
  let y = x + 1;
  if (y > 1000) return false;
  return x === 0;
}

function helperReturnString(s) {
  return s + "";
}

function helperReturnTaggedSmi(which) {
  if (which === 0) return 0;
  if (which === 1) return smiBox.a;
  return smiBox.b;
}

function helperReturnTaggedObject(which) {
  if (which === 0) return { tagProp: 22 };
  if (which === 1) return { tagProp: 22 };
  return { tagProp: 22 };
}

function helperReturnObject(o) {
  return { v: o };
}

function helperReturnNullOrUndefined(which) {
  return which ? null : undefined;
}

function helperReturnFloat64(x, y) {
  return x + y;
}

function helperReturnUint32(x) {
  return x >>> 0;
}

function helperReturnHoleyDouble(arr, i) {
  return arr[i];
}

function testBranches(x, s, u, arr, idx) {
  let acc = 0;
  if (helperReturnBoolean(x)) acc += 1;
  if (helperReturnString(s)) acc += 10;
  if (helperReturnTaggedSmi(x)) acc += 100;
  if (helperReturnObject(x)) acc += 1000;
  if (helperReturnNullOrUndefined(x === 0)) acc += 10000;
  if (helperReturnFloat64(u, 0.0)) acc += 100000;
  if (helperReturnUint32(u)) acc += 1000000;
  if (helperReturnHoleyDouble(arr, idx)) acc += 10000000;
  return acc;
}

// Exercise VisitBranchIfSmi folding to both true (s) and false (o).
Number.prototype.tagProp = 11;
function polyGetTag(v) {
  return v.tagProp;
}
function testBranchIfSmi(x) {
  const s = helperReturnTaggedSmi(x);
  const o = helperReturnTaggedObject(x);
  return polyGetTag(s) + polyGetTag(o);
}

// Regression tests: reusing a Float64 alternative or emitting UnsafeSmiUntag on
// a Phi that also has a TruncatedInt32 use must record the non-truncating Phi
// hint.
function id(v) {
  return v;
}
function testPhiTruncatedHintFloat64(cond, d) {
  let bounded = (d & 0xff) + 0.5;
  let phi = cond ? bounded : 0;
  let bit = (phi + 1) | 0;
  let y = id(phi);
  if (y) return 42 + bit;
  return bit;
}
function testPhiTruncatedHintSmi(cond, x) {
  let s = helperReturnTaggedSmi(x);
  let phi = cond ? s : 0;
  let bit = (phi + 1) | 0;
  let y = id(phi);
  if (y) return 42 + bit;
  return bit;
}

const holeyArr = [1.5, , -0.0, NaN];

const helpers = [
  helperReturnBoolean,
  helperReturnString,
  helperReturnTaggedSmi,
  helperReturnTaggedObject,
  helperReturnObject,
  helperReturnNullOrUndefined,
  helperReturnFloat64,
  helperReturnUint32,
  helperReturnHoleyDouble,
  polyGetTag,
  id,
];
const targets = [
  testBranches,
  testBranchIfSmi,
  testPhiTruncatedHintFloat64,
  testPhiTruncatedHintSmi,
];

for (const f of [...helpers, ...targets]) {
  %PrepareFunctionForOptimization(f);
}

polyGetTag(1);
polyGetTag({ tagProp: 22 });

function runChecks() {
  assertEquals(10001001, testBranches(0, "", 0.0, holeyArr, 0));
  assertEquals(1110, testBranches(1, "hello", -0.0, holeyArr, 1));
  assertEquals(1101110, testBranches(2, "x", 0x80000000, holeyArr, 2));
  assertEquals(1101110, testBranches(2, "x", 0xffffffff, holeyArr, 2));
  assertEquals(1110, testBranches(1, "x", NaN, holeyArr, 3));
  assertEquals(33, testBranchIfSmi(0));
  assertEquals(33, testBranchIfSmi(1));
  assertEquals(33, testBranchIfSmi(2));
  assertEquals(43, testPhiTruncatedHintFloat64(true, 0));
  assertEquals(1, testPhiTruncatedHintFloat64(false, 0));
  assertEquals(50, testPhiTruncatedHintSmi(true, 1));
  assertEquals(1, testPhiTruncatedHintSmi(false, 1));
}

for (let i = 0; i < 5; ++i) {
  runChecks();
}

for (const f of targets) %OptimizeMaglevOnNextCall(f);
runChecks();
for (const f of targets) {
  if (!isTurboFanned(f)) assertMaglevved(f);
}

for (const f of targets) %OptimizeFunctionOnNextCall(f);
runChecks();
for (const f of targets) assertOptimized(f);

// Verify NoUndetectableObjectsProtector invalidation deopts folded kJSReceiver
// with unknown maps, while post-invalidation compilation lowers to
// BranchIfUndetectable and fresh-map allocations still fold without protector.
function Ctor(flag) {
  return flag ? %GetUndetectable() : { a: 1 };
}
function Ctor2() {
  this.a = 1;
}
function makeObjOrUndetectable(ctor, flag) {
  return new ctor(flag);
}
function testUndetectable(ctor, flag) {
  const o = makeObjOrUndetectable(ctor, flag);
  return o ? 1 : 0;
}
function testUndetectableAfterInvalidation(ctor, flag) {
  const o = makeObjOrUndetectable(ctor, flag);
  const fresh = helperReturnObject(flag);
  return (o ? 1 : 0) + (fresh ? 10 : 0);
}

%PrepareFunctionForOptimization(Ctor);
%PrepareFunctionForOptimization(Ctor2);
%PrepareFunctionForOptimization(makeObjOrUndetectable);
%PrepareFunctionForOptimization(testUndetectable);
makeObjOrUndetectable(Ctor2, false);
for (let i = 0; i < 5; ++i) {
  assertEquals(1, testUndetectable(Ctor, false));
}
%OptimizeMaglevOnNextCall(testUndetectable);
assertEquals(1, testUndetectable(Ctor, false));
if (!isTurboFanned(testUndetectable)) assertMaglevved(testUndetectable);
assertEquals(0, testUndetectable(Ctor, true));
assertUnoptimized(testUndetectable);

%PrepareFunctionForOptimization(testUndetectableAfterInvalidation);
for (let i = 0; i < 5; ++i) {
  assertEquals(11, testUndetectableAfterInvalidation(Ctor, false));
  assertEquals(10, testUndetectableAfterInvalidation(Ctor, true));
}
%OptimizeMaglevOnNextCall(testUndetectableAfterInvalidation);
assertEquals(11, testUndetectableAfterInvalidation(Ctor, false));
assertEquals(10, testUndetectableAfterInvalidation(Ctor, true));
if (!isTurboFanned(testUndetectableAfterInvalidation)) {
  assertMaglevved(testUndetectableAfterInvalidation);
}
%OptimizeFunctionOnNextCall(testUndetectableAfterInvalidation);
assertEquals(11, testUndetectableAfterInvalidation(Ctor, false));
assertEquals(10, testUndetectableAfterInvalidation(Ctor, true));
assertOptimized(testUndetectableAfterInvalidation);
