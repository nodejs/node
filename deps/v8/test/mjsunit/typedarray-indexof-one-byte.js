// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Tests indexOf and includes on typed arrays with one-byte elements. These
// methods have a separate search path for one-byte elements in unshared
// buffers.

// Checks that indexOf, includes, and lastIndexOf agree with a scan that
// uses strict equality.
function check(ta, value, from) {
  let expected = -1;
  const len = ta.length;
  let k = from === undefined ? 0 : Math.trunc(from) || 0;
  if (k < 0) k = Math.max(len + k, 0);
  for (; k < len; k++) {
    if (ta[k] === value) {
      expected = k;
      break;
    }
  }
  assertEquals(expected, ta.indexOf(value, from));
  assertEquals(expected !== -1, ta.includes(value, from));
  if (from === undefined) {
    let last = -1;
    for (let i = len - 1; i >= 0; i--) {
      if (ta[i] === value) {
        last = i;
        break;
      }
    }
    assertEquals(last, ta.lastIndexOf(value));
  }
}

function fill(ta) {
  for (let i = 0; i < ta.length; i++) ta[i] = i;
  return ta;
}

const ctors = [Int8Array, Uint8Array, Uint8ClampedArray];
const values = [0, -0, 1, 7, 127, 128, -128, -1, 255, 256, -129, 0.5, 1.5,
                NaN, Infinity, -Infinity, '1', 1n, undefined, null];

for (const ctor of ctors) {
  const buffers = [new ArrayBuffer(300), new SharedArrayBuffer(300)];
  for (const buffer of buffers) {
    const ta = fill(new ctor(buffer));
    for (const value of values) {
      check(ta, value);
      for (const from of [-300, -1, 0, 5, 128, 254.9, 255, 300, -Infinity,
                          Infinity, NaN]) {
        check(ta, value, from);
      }
    }
    // The result is relative to the start of the view.
    const view = new ctor(buffer, 3, 100);
    for (const value of values) check(view, value);
  }
}

// Search values that are out of range for the element type must not match
// the byte that they would wrap to.
(function TestNoWrapAround() {
  const i8 = new Int8Array([-1, -128, 0]);
  assertEquals(-1, i8.indexOf(255));
  assertEquals(-1, i8.indexOf(128));
  assertEquals(0, i8.indexOf(-1));
  assertEquals(1, i8.indexOf(-128));
  assertFalse(i8.includes(255));

  const u8 = new Uint8Array([255, 128, 0]);
  assertEquals(-1, u8.indexOf(-1));
  assertEquals(-1, u8.indexOf(-128));
  assertEquals(-1, u8.indexOf(256));
  assertEquals(0, u8.indexOf(255));
  assertEquals(2, u8.indexOf(-0));
  assertFalse(u8.includes(-1));

  const clamped = new Uint8ClampedArray([255, 0, 1]);
  assertEquals(-1, clamped.indexOf(300));
  assertEquals(-1, clamped.indexOf(-5));
  assertEquals(-1, clamped.indexOf(0.5));
  assertEquals(-1, clamped.indexOf(1.5));
  assertEquals(0, clamped.indexOf(255));
  assertFalse(clamped.includes(300));
})();

// Long arrays, so that a vectorized search covers many blocks.
(function TestLongArray() {
  for (const ctor of ctors) {
    const ta = new ctor(65536 + 3);
    ta.fill(7);
    assertEquals(-1, ta.indexOf(9));
    assertFalse(ta.includes(9));
    ta[ta.length - 1] = 9;
    assertEquals(ta.length - 1, ta.indexOf(9));
    assertTrue(ta.includes(9));
    assertEquals(ta.length - 1, ta.indexOf(9, ta.length - 1));
    assertEquals(-1, ta.indexOf(9, ta.length));
    ta[1000] = 9;
    assertEquals(1000, ta.indexOf(9));
    assertEquals(ta.length - 1, ta.indexOf(9, 1001));
  }
})();

// fromIndex is converted after the length is read. Its valueOf can shrink,
// grow, or detach the buffer.
(function TestResizeInFromIndex() {
  for (const ctor of ctors) {
    // Shrink a length-tracking view. resize() sets the removed bytes to 0. If
    // the search reads past the new length, the search for 0 finds a match.
    let rab = new ArrayBuffer(16, {maxByteLength: 32});
    let ta = new ctor(rab);
    ta.fill(5);
    let from = {valueOf() { rab.resize(8); return 0; }};
    assertEquals(-1, ta.indexOf(0, from));
    rab.resize(16);
    ta.fill(5);
    assertFalse(ta.includes(0, {valueOf() { rab.resize(8); return 0; }}));

    // Grow a length-tracking view. Only the original length is searched.
    rab = new ArrayBuffer(16, {maxByteLength: 32});
    ta = new ctor(rab);
    from = {valueOf() { rab.resize(32); ta[20] = 5; return 0; }};
    assertEquals(-1, ta.indexOf(5, from));
    assertEquals(20, ta.indexOf(5));
    rab.resize(16);
    assertFalse(ta.includes(5, {valueOf() {
      rab.resize(32);
      ta[20] = 5;
      return 0;
    }}));

    // Shrink a fixed-length view out of bounds.
    rab = new ArrayBuffer(16, {maxByteLength: 32});
    ta = new ctor(rab, 0, 16);
    ta[3] = 5;
    from = {valueOf() { rab.resize(8); return 0; }};
    assertEquals(-1, ta.indexOf(5, from));

    // Detach.
    const ab = new ArrayBuffer(16);
    ta = new ctor(ab);
    ta[3] = 5;
    from = {valueOf() { %ArrayBufferDetach(ab); return 0; }};
    assertEquals(-1, ta.indexOf(5, from));
  }
})();

// Growable SharedArrayBuffer.
(function TestGrowableShared() {
  for (const ctor of ctors) {
    const gsab = new SharedArrayBuffer(16, {maxByteLength: 32});
    const ta = fill(new ctor(gsab));
    assertEquals(15, ta.indexOf(15));
    assertEquals(-1, ta.indexOf(20));
    gsab.grow(32);
    ta[20] = 20;
    assertEquals(20, ta.indexOf(20));
    assertTrue(ta.includes(20));
  }
})();

// fromIndex.valueOf shrinks a length-tracking view below the converted index.
// As in TestResizeInFromIndex, the search for 0 finds a match if the search
// reads the removed bytes.
(function TestShrinkBelowFromIndex() {
  for (const ctor of ctors) {
    for (const from of [8, 9, 12]) {
      const rab = new ArrayBuffer(16, {maxByteLength: 16});
      const ta = new ctor(rab);
      ta.fill(5);
      const index = {valueOf() { rab.resize(8); return from; }};
      assertEquals(-1, ta.indexOf(0, index));
      rab.resize(16);
      ta.fill(5);
      const index2 = {valueOf() { rab.resize(8); return from; }};
      assertFalse(ta.includes(0, index2));
    }
  }
})();

// Array.prototype.indexOf and includes reach the same code through the
// generic path, also with a length that is larger than the typed array.
(function TestArrayPrototypeMethods() {
  for (const ctor of ctors) {
    const ta = new ctor([1, 2, 3, 7]);
    assertEquals(3, Array.prototype.indexOf.call(ta, 7));
    assertTrue(Array.prototype.includes.call(ta, 7));
    assertEquals(-1, Array.prototype.indexOf.call(ta, 9));
    assertFalse(Array.prototype.includes.call(ta, 9));
    Object.defineProperty(ta, 'length', {value: 100});
    assertEquals(3, Array.prototype.indexOf.call(ta, 7));
    assertEquals(-1, Array.prototype.indexOf.call(ta, 9));
    assertFalse(Array.prototype.includes.call(ta, 9));
    assertTrue(Array.prototype.includes.call(ta, undefined));
  }
})();
