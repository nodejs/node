// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

function sortDescending(a) {
  return a.sort((x, y) => (y | 0) - (x | 0));
}

%PrepareFunctionForOptimization(sortDescending);
assertEquals([5, 4, 3, 2, 1], sortDescending([3, 1, 5, 2, 4]));
assertEquals([10, 0, -5], sortDescending([-5, 10, 0]));
%OptimizeFunctionOnNextCall(sortDescending);
assertEquals([9, 7, 5, 3, 1], sortDescending([1, 9, 3, 7, 5]));
assertOptimized(sortDescending);

// Number feedback makes the inlined compares Float64 compares of the
// converted int32/uint32 arguments.
function lt(x, y) { return x < y; }
function le(x, y) { return x <= y; }
function eq(x, y) { return x == y; }
%PrepareFunctionForOptimization(lt);
%PrepareFunctionForOptimization(le);
%PrepareFunctionForOptimization(eq);
lt(0.5, 1.5);
le(0.5, 1.5);
eq(0.5, 1.5);

// Compare int32-converted values against each other and against Float64
// constants, including -0.0, INT32_MIN, INT32_MAX, a non-integer and a
// uint32-only value.
function compareInt32AsFloat64(a, b) {
  let ia = a | 0;
  let ib = b | 0;
  return [
    lt(ia, 0.0),
    le(ia, -0.0),
    eq(ia, -0.0),
    lt(ia, ib),
    le(ia, ib),
    eq(ia, ib),
    lt(ia, 2147483647.0),
    le(-2147483648.0, ia),
    lt(ia, 1.5),
    eq(ia, 4294967295.0),
  ];
}

%PrepareFunctionForOptimization(compareInt32AsFloat64);
compareInt32AsFloat64(0, 1);
compareInt32AsFloat64(-1, 0);
compareInt32AsFloat64(2147483647, -2147483648);
%OptimizeFunctionOnNextCall(compareInt32AsFloat64);
assertEquals(
    [false, true, true, true, true, false, true, true, true, false],
    compareInt32AsFloat64(0, 1));
assertEquals(
    [true, true, false, false, true, true, true, true, true, false],
    compareInt32AsFloat64(-2147483648, -2147483648));
assertEquals(
    [false, false, false, false, true, true, false, true, false, false],
    compareInt32AsFloat64(2147483647, 2147483647));
assertEquals(
    [false, false, false, false, false, false, true, true, false, false],
    compareInt32AsFloat64(2, 1));
assertEquals(
    [true, true, false, false, true, true, true, true, true, false],
    compareInt32AsFloat64(-1, -1));
assertOptimized(compareInt32AsFloat64);

// Same for uint32-converted values, including values >= 2^31 for which signed
// and unsigned ordering differ.
function compareUint32AsFloat64(a, b) {
  let ua = a >>> 0;
  let ub = b >>> 0;
  return [
    lt(ua, ub),
    le(ua, ub),
    eq(ua, ub),
    lt(ua, 4294967295.0),
    le(ua, 4294967295.0),
    eq(ua, 4294967295.0),
    lt(ua, 2147483648.0),
    eq(ua, 2147483648.0),
    lt(ua, -1.0),
    le(ua, -0.0),
    eq(ua, -0.0),
    lt(ua, 1.5),
    eq(ua, -1.0),
  ];
}

%PrepareFunctionForOptimization(compareUint32AsFloat64);
compareUint32AsFloat64(0, 1);
compareUint32AsFloat64(0xFFFFFFFF, 0x80000000);
compareUint32AsFloat64(0x80000000, 0x7FFFFFFF);
%OptimizeFunctionOnNextCall(compareUint32AsFloat64);
assertEquals(
    [false, false, false, false, true, true, false, false, false, false, false,
     false, false],
    compareUint32AsFloat64(0xFFFFFFFF, 0x80000000));
assertEquals(
    [false, false, false, true, true, false, false, true, false, false, false,
     false, false],
    compareUint32AsFloat64(0x80000000, 0x7FFFFFFF));
assertEquals(
    [true, true, false, true, true, false, true, false, false, true, true,
     true, false],
    compareUint32AsFloat64(0, -1));
assertEquals(
    [false, true, true, true, true, false, true, false, false, false, false,
     false, false],
    compareUint32AsFloat64(0x7FFFFFFF, 0x7FFFFFFF));
assertOptimized(compareUint32AsFloat64);

// Mixed signed and unsigned inputs must not be compared as Word32.
function compareMixedAsFloat64(a, b) {
  let ia = a | 0;
  let ub = b >>> 0;
  return [lt(ia, ub), le(ia, ub), eq(ia, ub)];
}

%PrepareFunctionForOptimization(compareMixedAsFloat64);
compareMixedAsFloat64(0, 1);
compareMixedAsFloat64(-1, 0xFFFFFFFF);
compareMixedAsFloat64(0x80000000, 0x80000000);
%OptimizeFunctionOnNextCall(compareMixedAsFloat64);
assertEquals([true, true, false], compareMixedAsFloat64(-1, 0xFFFFFFFF));
assertEquals([false, true, true], compareMixedAsFloat64(0, 0));
assertEquals([false, false, false], compareMixedAsFloat64(5, 3));
assertEquals(
    [true, true, false], compareMixedAsFloat64(0x80000000, 0x80000000));
assertOptimized(compareMixedAsFloat64);
