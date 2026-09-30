// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Fast-path lastIndexOf: elements-kind × search-type combinations, including
// fromIndex -1 and searches that must return -1.

function assertPackedSmi(arr) {
  assertTrue(%HasSmiElements(arr));
  assertFalse(%HasHoleyElements(arr));
}

function assertHoleySmi(arr) {
  assertTrue(%HasSmiElements(arr));
  assertTrue(%HasHoleyElements(arr));
}

function assertPackedDouble(arr) {
  assertTrue(%HasDoubleElements(arr));
  assertFalse(%HasHoleyElements(arr));
}

function assertHoleyDouble(arr) {
  assertTrue(%HasDoubleElements(arr));
  assertTrue(%HasHoleyElements(arr));
}

function assertPackedObject(arr) {
  assertTrue(%HasObjectElements(arr));
  assertFalse(%HasHoleyElements(arr));
}

function assertHoleyObject(arr) {
  assertTrue(%HasObjectElements(arr));
  assertTrue(%HasHoleyElements(arr));
}

(function packedSmi() {
  var a = [1, 2, -1, 0, 2];
  assertPackedSmi(a);

  assertEquals(4, a.lastIndexOf(2));
  assertEquals(4, a.lastIndexOf(2, -1));
  assertEquals(1, a.lastIndexOf(2, -2));
  assertEquals(2, a.lastIndexOf(-1));
  assertEquals(2, a.lastIndexOf(-1, -1));
  assertEquals(3, a.lastIndexOf(0));
  assertEquals(3, a.lastIndexOf(-0));
  assertEquals(-1, a.lastIndexOf(3));
  assertEquals(-1, a.lastIndexOf(NaN));
  assertEquals(-1, a.lastIndexOf(1.5));
  assertEquals(-1, a.lastIndexOf(undefined));
  assertEquals(-1, a.lastIndexOf('2'));
  assertEquals(-1, a.lastIndexOf(true));
  assertEquals(-1, [].lastIndexOf(-1));
})();

(function holeySmi() {
  var a = [1, , -1, 0, 2];
  assertHoleySmi(a);

  assertEquals(4, a.lastIndexOf(2, -1));
  assertEquals(2, a.lastIndexOf(-1));
  assertEquals(-1, a.lastIndexOf(undefined));
  assertEquals(-1, a.lastIndexOf(NaN));
  assertEquals(-1, a.lastIndexOf('1'));
})();

(function packedDouble() {
  var a = [1.5, 2, -1, 0, 2.5, NaN];
  assertPackedDouble(a);

  assertEquals(4, a.lastIndexOf(2.5));
  assertEquals(4, a.lastIndexOf(2.5, -1));
  assertEquals(1, a.lastIndexOf(2));
  assertEquals(2, a.lastIndexOf(-1));
  assertEquals(2, a.lastIndexOf(-1, -1));
  assertEquals(3, a.lastIndexOf(0));
  assertEquals(3, a.lastIndexOf(-0));
  assertEquals(0, a.lastIndexOf(1.5));
  assertEquals(-1, a.lastIndexOf(NaN));
  assertEquals(-1, a.lastIndexOf(undefined));
  assertEquals(-1, a.lastIndexOf('2'));
  assertEquals(-1, a.lastIndexOf(true));
  assertEquals(-1, a.lastIndexOf({}));
})();

(function holeyDouble() {
  var a = [1.5, , -1, 0, 2.5];
  assertHoleyDouble(a);

  assertEquals(4, a.lastIndexOf(2.5, -1));
  assertEquals(2, a.lastIndexOf(-1));
  assertEquals(-1, a.lastIndexOf(undefined));
  assertEquals(-1, a.lastIndexOf(NaN));
  assertEquals(-1, a.lastIndexOf('1.5'));
})();

(function holeyDoubleUndefined() {
  var a = [1.5, 2.5, 3.5];
  a[1] = undefined;
  if (%IsUndefinedDoubleEnabled()) {
    assertHoleyDouble(a);
  } else {
    assertTrue(%HasObjectElements(a));
  }
  assertEquals(1, a.lastIndexOf(undefined));
  assertEquals(1, a.lastIndexOf(undefined, -1));
  assertEquals(-1, a.lastIndexOf(undefined, 0));
  assertEquals(-1, a.lastIndexOf(NaN));
})();

(function packedObject() {
  var o = {};
  var a = [1, 'x', 1.5, -1, o, 'x', 0];
  assertPackedObject(a);

  assertEquals(5, a.lastIndexOf('x'));
  assertEquals(5, a.lastIndexOf('x', -1));
  assertEquals(5, a.lastIndexOf('x', -2));
  assertEquals(3, a.lastIndexOf(-1));
  assertEquals(0, a.lastIndexOf(1));
  assertEquals(2, a.lastIndexOf(1.5));
  assertEquals(6, a.lastIndexOf(0));
  assertEquals(6, a.lastIndexOf(-0));
  assertEquals(4, a.lastIndexOf(o));
  assertEquals(-1, a.lastIndexOf({}));
  assertEquals(-1, a.lastIndexOf(NaN));
  assertEquals(-1, a.lastIndexOf(undefined));
  assertEquals(-1, a.lastIndexOf(true));
})();

(function holeyObject() {
  var a = [1, , 'x', -1, undefined];
  assertHoleyObject(a);

  assertEquals(3, a.lastIndexOf(-1, -1));
  assertEquals(2, a.lastIndexOf('x'));
  assertEquals(4, a.lastIndexOf(undefined));
  assertEquals(-1, a.lastIndexOf(NaN));
})();

(function mixedNumberObject() {
  // PACKED_ELEMENTS holding both Smi and HeapNumber.
  var a = [1, 'x', 2.5, 2];
  assertPackedObject(a);
  assertEquals(3, a.lastIndexOf(2));
  assertEquals(2, a.lastIndexOf(2.5));
  assertEquals(0, a.lastIndexOf(1));
  assertEquals(-1, a.lastIndexOf(-1));
})();
