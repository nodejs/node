// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

// 1. Duplicate tagging conversions (Float64ToTagged, Int32ToNumber,
// HoleyFloat64ToTagged, Uint32ToNumber) combined with CSE, untagged reuse,
// and loop backedge KNA merging.
function testTaggingCseAndReuse(arr, holeyArr, n) {
  let sum = 0;
  let boxA = {v: 1.5};
  let boxB = {v: 2.5};
  boxA.v = 3.5;
  for (let i = 0; i < n; i++) {
    // Float64 tagging + CSE + untagged reuse.
    let f = arr[i & 1] + 0.25;
    boxA.v = f;
    boxB.v = f;
    sum += boxA.v + boxB.v;

    // HoleyFloat64 tagging + ToBoolean.
    let hf = holeyArr[i & 1];
    if (hf) {
      sum += hf;
    }

    // Smi-ranged Int32ToNumber overwritten in-place to UnsafeSmiTagInt32 and
    // deduplicated across multiple tagged uses.
    let smi = (i + 1) & 7;
    boxA.s = smi;
    boxB.s = smi;
    sum += boxA.s;

    // Uint32ToNumber in Smi range and full Uint32 range.
    let uSmi = (i >>> 0) & 3;
    let uBig = (i + 0x80000000) >>> 0;
    if (uBig) {
      sum += uSmi;
    }
  }
  return sum;
}

const doubleArr = [1.5, 2.5];
const holeyDoubleArr = [1.5, , 3.5];
%PrepareFunctionForOptimization(testTaggingCseAndReuse);
assertEquals(14.5, testTaggingCseAndReuse(doubleArr, holeyDoubleArr, 2));
assertEquals(14.5, testTaggingCseAndReuse(doubleArr, holeyDoubleArr, 2));
%OptimizeFunctionOnNextCall(testTaggingCseAndReuse);
assertEquals(14.5, testTaggingCseAndReuse(doubleArr, holeyDoubleArr, 2));
assertOptimized(testTaggingCseAndReuse);

// 2. ToBoolean / LogicalNot folding across tagging conversions, ReturnedValue,
// holey double elements, and CheckedObjectToIndex("0") -> 0 -> false.
function inlinedInt32(c, x) {
  return c ? (x | 0) : ((x + 1) | 0);
}
%PrepareFunctionForOptimization(inlinedInt32);

function testToBooleanOnConversions(c, x, holeyArr, idx, keyedArr, key) {
  const bInt32 = !inlinedInt32(c, x);
  const bFloat64 = Boolean((x + 0.5) - 0.5);
  const bHoley = Boolean(holeyArr[idx]);
  const bUint32 = Boolean(x >>> 0);
  // CheckedObjectToIndex converts string "0" to Int32 0; if that index is
  // re-tagged to Number, ToBoolean must see 0 (false), not "0" (true).
  let v = keyedArr[key];
  const bIndex = Boolean(+key);
  return [bInt32, bFloat64, bHoley, bUint32, bIndex, v];
}

const keyed = [42, 99];
%PrepareFunctionForOptimization(testToBooleanOnConversions);
assertEquals(
    [true, false, false, false, false, 42],
    testToBooleanOnConversions(true, 0, holeyDoubleArr, 1, keyed, '0'));
assertEquals(
    [false, true, true, true, true, 99],
    testToBooleanOnConversions(true, 1, holeyDoubleArr, 0, keyed, 1));
%OptimizeFunctionOnNextCall(testToBooleanOnConversions);
assertEquals(
    [true, false, false, false, false, 42],
    testToBooleanOnConversions(true, 0, holeyDoubleArr, 1, keyed, '0'));
assertEquals(
    [false, true, true, true, true, 99],
    testToBooleanOnConversions(true, 1, holeyDoubleArr, 0, keyed, 1));

// 3. CheckedSmiUntag on a loop phi that PhiRepresentationSelector untags to
// Int32 (rewriting CheckedSmiUntag -> CheckedSmiSizedInt32 in backedge KNA).
const smiBox = {x: 1};
smiBox.x = 2;
function testLoopPhiSmiCheckRemap(n) {
  let acc = 0;
  for (let i = 0; i < n; i = (i + 1) | 0) {
    smiBox.x = acc;
    acc = (acc + 1) | 0;
  }
  return smiBox.x;
}

%PrepareFunctionForOptimization(testLoopPhiSmiCheckRemap);
assertEquals(2, testLoopPhiSmiCheckRemap(3));
assertEquals(3, testLoopPhiSmiCheckRemap(4));
%OptimizeFunctionOnNextCall(testLoopPhiSmiCheckRemap);
assertEquals(4, testLoopPhiSmiCheckRemap(5));
assertOptimized(testLoopPhiSmiCheckRemap);

// 4. CheckedSmiTagHoleyFloat64 KNA tracking (storing a HoleyFloat64 element
// into a Smi-representation field, then reusing its tagged alternative).
const smiHolder = {s: 1};
smiHolder.s = 2;
function storeSmiField(o, v) {
  if (o !== null) {
    o.s = v;
  }
}
%PrepareFunctionForOptimization(storeSmiField);
storeSmiField(smiHolder, 3);
storeSmiField(smiHolder, 4);
storeSmiField(null, 0);

const smiHoleyDoubles = [10.5, , 20];
smiHoleyDoubles[0] = 10;
function testCheckedSmiTagHoleyFloat64(o, a, idx) {
  const v = a[idx];
  storeSmiField(o, v);
  return v;
}

%PrepareFunctionForOptimization(testCheckedSmiTagHoleyFloat64);
assertEquals(10, testCheckedSmiTagHoleyFloat64(null, smiHoleyDoubles, 0));
assertEquals(20, testCheckedSmiTagHoleyFloat64(null, smiHoleyDoubles, 2));
%OptimizeFunctionOnNextCall(testCheckedSmiTagHoleyFloat64);
assertEquals(10, testCheckedSmiTagHoleyFloat64(smiHolder, smiHoleyDoubles, 0));
assertEquals(10, smiHolder.s);
assertOptimized(testCheckedSmiTagHoleyFloat64);
// Reading the hole fails CheckedSmiTagHoleyFloat64, deopts, and stores
// undefined.
assertEquals(
    undefined, testCheckedSmiTagHoleyFloat64(smiHolder, smiHoleyDoubles, 1));
assertEquals(undefined, smiHolder.s);
