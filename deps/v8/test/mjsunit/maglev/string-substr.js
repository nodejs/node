// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --turbolev --expose-externalize-string

function substr0(str) {
  return str.substr();
}

function substr1(str, start) {
  return str.substr(start);
}

function substr2(str, start, len) {
  return str.substr(start, len);
}

const s = 'abcdefg';
const ext = createExternalizableString('Hello, World!');
externalizeString(ext);

%PrepareFunctionForOptimization(substr0);
%PrepareFunctionForOptimization(substr1);
%PrepareFunctionForOptimization(substr2);
for (let i = 0; i < 10; i++) {
  assertEquals(s, substr0(s));
  assertEquals('bcdefg', substr1(s, 1));
  assertEquals('bc', substr2(s, 1, 2));
}

%OptimizeFunctionOnNextCall(substr0);
%OptimizeFunctionOnNextCall(substr1);
%OptimizeFunctionOnNextCall(substr2);

assertEquals(s, substr0(s));
assertEquals('Hello, World!', substr0(ext));
assertOptimized(substr0);

assertEquals('bcdefg', substr1(s, 1));
assertEquals(s, substr1(s, undefined));
assertEquals('fg', substr1(s, -2));
assertEquals(s, substr1(s, -100));
assertEquals('', substr1(s, 100));
assertEquals('World!', substr1(ext, 7));
assertEquals('World!', substr1(ext, -6));
assertOptimized(substr1);

assertEquals('bc', substr2(s, 1, 2));
assertEquals('ab', substr2(s, undefined, 2));
assertEquals(s, substr2(s, undefined, undefined));
assertEquals('fg', substr2(s, -2, 10));
assertEquals('ab', substr2(s, -100, 2));
assertEquals('', substr2(s, 100, 2));
assertEquals('', substr2(s, 1, 0));
assertEquals('', substr2(s, 1, -5));
assertEquals('bcdefg', substr2(s, 1, undefined));
assertEquals('Hello', substr2(ext, 0, 5));
assertEquals('World', substr2(ext, -6, 5));
assertEquals('World!', substr2(ext, 7, undefined));
assertOptimized(substr2);

// Holey double array element (HoleyFloat64 undefined / int32 path).
const holeyLengths = [2, , 3, 0.5, NaN];
assertTrue(%HasDoubleElements(holeyLengths));
assertTrue(%HasHoleyElements(holeyLengths));
function substrHoleyDouble(str, idx) {
  return str.substr(1, holeyLengths[idx]);
}
%PrepareFunctionForOptimization(substrHoleyDouble);
for (let i = 0; i < 10; i++) {
  assertEquals('bc', substrHoleyDouble(s, 0));
  assertEquals('bcdefg', substrHoleyDouble(s, 1));
  assertEquals('bcd', substrHoleyDouble(s, 2));
}
%OptimizeFunctionOnNextCall(substrHoleyDouble);
assertEquals('bc', substrHoleyDouble(s, 0));
assertEquals('bcdefg', substrHoleyDouble(s, 1));
assertEquals('bcd', substrHoleyDouble(s, 2));
assertOptimized(substrHoleyDouble);
// A quiet NaN must not take the undefined branch.
assertEquals('', substrHoleyDouble(s, 3));
assertEquals('', substrHoleyDouble(s, 4));

// INT32_MIN start (unboxed int32 path).
function substrInt32(str, start, len) {
  return str.substr(start | 0, len | 0);
}
%PrepareFunctionForOptimization(substrInt32);
for (let i = 0; i < 10; i++) {
  assertEquals('ab', substrInt32(s, -2147483648, 2));
}
%OptimizeFunctionOnNextCall(substrInt32);
assertEquals('ab', substrInt32(s, -2147483648, 2));
assertOptimized(substrInt32);

// Non-Smi int32 values computed in optimized code.
function substrLenMax(str, x) {
  return str.substr(0, x * 2 + 1);
}
function substrStartLenMax(str, x) {
  return str.substr(1, x * 2 + 1);
}
function substrStartMax(str, x) {
  return str.substr(x * 2 + 1, 3);
}
function substrLenMin(str, x) {
  return str.substr(1, x * -2 - 2);
}
function substrStartMin(str, x) {
  return str.substr(x * -2 - 2, 2);
}
const kHalfInt32Max = 2 ** 30 - 1;
for (const [f, expected] of [
       [substrLenMax, 'abcdefg'], [substrStartLenMax, 'bcdefg'],
       [substrStartMax, ''], [substrLenMin, ''], [substrStartMin, 'ab']]) {
  %PrepareFunctionForOptimization(f);
  for (let i = 0; i < 10; i++) f(s, 1);
  %OptimizeFunctionOnNextCall(f);
  f(s, 1);
  assertOptimized(f);
  assertEquals(expected, f(s, kHalfInt32Max));
  assertOptimized(f);
}

// Deopts on non-Smi arguments, and does not deopt again after
// re-optimization.
for (const [args, expected] of [
       [[s, 2147483647, 1], ''], [[s, 0, 2147483647], s],
       [[s, 1, -2147483648], ''], [[s, '', 2], 'ab'], [[s, 1, ''], ''],
       [[s, null, 2], 'ab'], [[s, 1, null], ''],
       [[{ substr() { return 'x'; } }, 0, 1], 'x']]) {
  // A unique source avoids sharing the function via the compilation cache.
  const substrDeopt = new Function(
      'str', 'start', 'len',
      `return str.substr(start, len); // ${JSON.stringify(args)}`);
  %PrepareFunctionForOptimization(substrDeopt);
  for (let i = 0; i < 10; i++) substrDeopt(s, 1, 2);
  %OptimizeFunctionOnNextCall(substrDeopt);
  assertEquals('bc', substrDeopt(s, 1, 2));
  assertOptimized(substrDeopt);
  assertEquals(expected, substrDeopt(...args));
  // INT32_MAX and INT32_MIN are Smis on configs with 32-bit Smis.
  if (typeof args[0] === 'string' && %IsSmi(args[1]) && %IsSmi(args[2])) {
    assertOptimized(substrDeopt);
  } else {
    assertUnoptimized(substrDeopt);
  }
  %OptimizeFunctionOnNextCall(substrDeopt);
  assertEquals(expected, substrDeopt(...args));
  assertOptimized(substrDeopt);
}
