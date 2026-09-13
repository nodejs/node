// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

const u8 = new Uint8Array(8);
const u16 = new Uint16Array(8);
const u32 = new Uint32Array(8);

// TEST 1: Uint16 element loads are in [0, 65535], which fits int32, so the
// checked int32 conversion of the load folds to an unchecked truncation.
function sum16(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    u16[i] = (u16[i] + i + 1) | 0;
    s = (s + u16[i]) | 0;
  }
  return s;
}
%PrepareFunctionForOptimization(sum16);
const expected16 = sum16(8);
u16.fill(0);
%OptimizeFunctionOnNextCall(sum16);
assertEquals(expected16, sum16(8));

// TEST 2: Uint32 element loads can exceed the int32 range, so the conversion
// must be kept and large values (> INT32_MAX) stay correct.
function read32(i) {
  return u32[i] + 1;
}
u32[0] = 0xfffffffe;
u32[1] = 5;
%PrepareFunctionForOptimization(read32);
assertEquals(0xffffffff, read32(0));
assertEquals(6, read32(1));
%OptimizeFunctionOnNextCall(read32);
assertEquals(0xffffffff, read32(0));
assertEquals(6, read32(1));

// TEST 3: codePointAt is in [0, 0x10FFFF], which fits int32, so the >>> 0 and
// the following modulus fold without deopting, including for a surrogate pair.
function xorIndex(a, b) {
  const x = (a.codePointAt(0) ^ b.codePointAt(0)) >>> 0;
  return x % 8;
}
%PrepareFunctionForOptimization(xorIndex);
assertEquals(("a".codePointAt(0) ^ "b".codePointAt(0)) % 8, xorIndex("a", "b"));
%OptimizeFunctionOnNextCall(xorIndex);
assertEquals(("a".codePointAt(0) ^ "b".codePointAt(0)) % 8, xorIndex("a", "b"));
assertEquals(("\u{1F0A1}".codePointAt(0) ^ "z".codePointAt(0)) % 8,
             xorIndex("\u{1F0A1}", "z"));

// TEST 4: (x | 0) >>> 0 reinterprets a possibly-negative int32 as uint32, so
// the range must not assume the value is non-negative.
function negToUint(x) {
  return (x | 0) >>> 0;
}
%PrepareFunctionForOptimization(negToUint);
assertEquals(4294967295, negToUint(-1));
assertEquals(7, negToUint(7));
%OptimizeFunctionOnNextCall(negToUint);
assertEquals(4294967295, negToUint(-1));
assertEquals(7, negToUint(7));

// TEST 5: charCodeAt out of bounds returns NaN. The first OOB access deopts the
// initial code; the re-optimized code then keeps an inline NaN fallback next to
// the in-bounds load, whose [0, 0xFFFF] range must stay sound across the merge.
function charCodeMod(s, i) {
  return (s.charCodeAt(i) >>> 0) % 8;
}
%PrepareFunctionForOptimization(charCodeMod);
assertEquals("A".charCodeAt(0) % 8, charCodeMod("A", 0));
%OptimizeFunctionOnNextCall(charCodeMod);
assertEquals("A".charCodeAt(0) % 8, charCodeMod("A", 0));
assertEquals(0, charCodeMod("A", 9));  // OOB: NaN >>> 0 -> 0.
%OptimizeFunctionOnNextCall(charCodeMod);
assertEquals("A".charCodeAt(0) % 8, charCodeMod("A", 0));
assertEquals(0, charCodeMod("A", 9));  // OOB now handled inline.

// TEST 6: codePointAt out of bounds returns undefined, and ToUint32(undefined)
// is 0. The in-bounds load keeps its [0, 0x10FFFF] range across the merge.
function codePointMod(s, i) {
  return (s.codePointAt(i) >>> 0) % 8;
}
%PrepareFunctionForOptimization(codePointMod);
assertEquals("A".codePointAt(0) % 8, codePointMod("A", 0));
%OptimizeFunctionOnNextCall(codePointMod);
assertEquals("A".codePointAt(0) % 8, codePointMod("A", 0));
assertEquals(0, codePointMod("A", 9));  // OOB: undefined >>> 0 -> 0.
%OptimizeFunctionOnNextCall(codePointMod);
assertEquals("A".codePointAt(0) % 8, codePointMod("A", 0));
assertEquals(0, codePointMod("A", 9));  // OOB now handled inline.

// TEST 7: A TypedArray out-of-bounds read returns undefined. The in-bounds
// Uint8 load keeps its [0, 255] range while the OOB branch stays correct.
function readU8(i) {
  return (u8[i] >>> 0) % 8;
}
u8[0] = 201;
%PrepareFunctionForOptimization(readU8);
assertEquals(201 % 8, readU8(0));
%OptimizeFunctionOnNextCall(readU8);
assertEquals(201 % 8, readU8(0));
assertEquals(0, readU8(9));  // OOB: undefined >>> 0 -> 0.
%OptimizeFunctionOnNextCall(readU8);
assertEquals(201 % 8, readU8(0));
assertEquals(0, readU8(9));  // OOB now handled inline.
