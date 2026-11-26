// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function setField(o, value) {
  o.field = value;
}

function readMarker(o) {
  return o.field.marker;
}

// Complete slack tracking and make `field` a mutable out-of-object HeapObject
// property, while generalizing subsequent out-of-object fields to Tagged.
for (let i = 0; i < 10; ++i) {
  const seed = {initial: 0};
  seed.field = 'a';
  seed.field = 'b';
  seed.p1 = 1;
  seed.p1 = 'a';
  seed.p2 = 1;
  seed.p2 = 'a';
  seed.p3 = 1;
  seed.p3 = 'a';
  seed.p4 = 1;
  seed.p4 = 'a';
  seed.p5 = 1;
  seed.p5 = 'a';
}

const holderWithMarker = {initial: 0};
holderWithMarker.field = {marker: 1.5};
holderWithMarker.p1 = 1;
holderWithMarker.p2 = 1;
holderWithMarker.p3 = 1;
holderWithMarker.p4 = 1;
holderWithMarker.p5 = 1;
holderWithMarker.left = 1;

// Case 1: Inner Float64 Phi merged with null into a Tagged Phi before
// PropertyArray extension. Keeping `o` non-escaping on the hot path lets
// EscapeAnalysis scalar-replace `o` and its initial PropertyArray while the
// diverging `left`/`right` transitions merge two extended PropertyArrays into
// an escaping Phi; the cold `materialize` branch then deoptimizes and exposes
// `o` with that extended PropertyArray.
function makeMergedWithNull(first, branch_left, n, materialize, store_null) {
  const o = {initial: 0};
  o.field = 's';
  o.p1 = 1;
  o.p2 = 1;
  o.p3 = 1;
  if (!store_null) {
    if (first) {
      setField(o, n * 0.5);
    } else {
      setField(o, n * 0.25);
    }
  } else {
    setField(o, null);
  }
  o.p4 = 1;
  o.p5 = 1;
  if (branch_left) {
    o.left = 1;
  } else {
    o.right = 1;
  }
  if (materialize) {
    // An unexecuted binary op has kNone feedback, so JSTypeHintLowering emits a
    // block-terminating SoftDeoptimize before `return o`, keeping `o`
    // non-escaping on the fast path while materializing it on deopt.
    n - 1;
    return o;
  }
  return 42;
}

// Case 2: Direct Float64 Phi (no null arm) feeding StoreField[kTaggedPointer]
// during PropertyArray extension.
function makeDirectFloatPhi(first, branch_left, n, materialize) {
  const o = {initial: 0};
  o.field = 's';
  o.p1 = 1;
  o.p2 = 1;
  o.p3 = 1;
  if (first) {
    setField(o, n * 0.5);
  } else {
    setField(o, n * 0.25);
  }
  o.p4 = 1;
  o.p5 = 1;
  if (branch_left) {
    o.left = 1;
  } else {
    o.right = 1;
  }
  if (materialize) {
    // An unexecuted binary op has kNone feedback, so JSTypeHintLowering emits a
    // block-terminating SoftDeoptimize before `return o`, keeping `o`
    // non-escaping on the fast path while materializing it on deopt.
    n - 1;
    return o;
  }
  return 42;
}

%PrepareFunctionForOptimization(setField);
%PrepareFunctionForOptimization(readMarker);
%PrepareFunctionForOptimization(makeMergedWithNull);
%PrepareFunctionForOptimization(makeDirectFloatPhi);

for (let i = 0; i < 20; ++i) {
  makeMergedWithNull(
      (i & 1) === 0, (i % 3) === 0, 1, false, (i & 2) !== 0);
  makeDirectFloatPhi((i & 1) === 0, (i % 3) === 0, 1, false);
  assertEquals(1.5, readMarker(holderWithMarker));
}

%OptimizeFunctionOnNextCall(readMarker);
assertEquals(1.5, readMarker(holderWithMarker));

%OptimizeFunctionOnNextCall(makeMergedWithNull);
assertEquals(42, makeMergedWithNull(true, true, 1, false, false));
const mergedObj = makeMergedWithNull(true, true, 134217728, true, false);
%HeapObjectVerify(mergedObj);
assertEquals(67108864, mergedObj.field);
assertEquals(undefined, readMarker(mergedObj));

// Verify the non-Smi (null) passthrough arm of ChangeTaggedToTaggedPointer.
const makeMergedNull = copyFunctionWithFreshFeedback(makeMergedWithNull);
%PrepareFunctionForOptimization(makeMergedNull);
for (let i = 0; i < 20; ++i) {
  makeMergedNull((i & 1) === 0, (i % 3) === 0, 1, false, (i & 2) !== 0);
}
%OptimizeFunctionOnNextCall(makeMergedNull);
const mergedNullObj = makeMergedNull(true, true, 1, true, true);
%HeapObjectVerify(mergedNullObj);
assertEquals(null, mergedNullObj.field);

const readMarker2 = copyFunctionWithFreshFeedback(readMarker);
%PrepareFunctionForOptimization(readMarker2);
for (let i = 0; i < 5; ++i) {
  assertEquals(1.5, readMarker2(holderWithMarker));
}
%OptimizeFunctionOnNextCall(readMarker2);
assertEquals(1.5, readMarker2(holderWithMarker));

%OptimizeFunctionOnNextCall(makeDirectFloatPhi);
assertEquals(42, makeDirectFloatPhi(true, true, 1, false));
const directObj = makeDirectFloatPhi(true, true, 134217728, true);
%HeapObjectVerify(directObj);
assertEquals(67108864, directObj.field);
assertEquals(undefined, readMarker2(directObj));

// Also cover negative Smi and 0 in Float64 -> TaggedPointer conversion.
for (const [n, expected] of [[-134217728, -67108864], [0, 0]]) {
  const makeDirect = copyFunctionWithFreshFeedback(makeDirectFloatPhi);
  const readM = copyFunctionWithFreshFeedback(readMarker);
  %PrepareFunctionForOptimization(makeDirect);
  %PrepareFunctionForOptimization(readM);
  for (let i = 0; i < 10; ++i) {
    makeDirect((i & 1) === 0, (i % 3) === 0, 1, false);
    assertEquals(1.5, readM(holderWithMarker));
  }
  %OptimizeFunctionOnNextCall(makeDirect);
  %OptimizeFunctionOnNextCall(readM);
  const obj = makeDirect(true, true, n, true);
  %HeapObjectVerify(obj);
  assertEquals(expected, obj.field);
  assertEquals(undefined, readM(obj));
}
