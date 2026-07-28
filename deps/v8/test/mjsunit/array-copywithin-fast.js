// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Exercises the fast path of Array.prototype.copyWithin (in-place MoveElements
// over a fast JSArray backing store) across every fast elements kind, plus the
// bailouts to the generic spec loop.

// PACKED_SMI_ELEMENTS, overlapping forward (to < from).
(function packedSmiForward() {
  assertArrayEquals([4, 5, 3, 4, 5], [1, 2, 3, 4, 5].copyWithin(0, 3));
  assertArrayEquals([1, 4, 5, 4, 5], [1, 2, 3, 4, 5].copyWithin(1, 3));
})();

// PACKED_SMI_ELEMENTS, overlapping backward (to > from): memmove must not
// clobber the source while copying.
(function packedSmiBackward() {
  assertArrayEquals([1, 2, 3, 1, 2], [1, 2, 3, 4, 5].copyWithin(3, 0));
  assertArrayEquals([1, 2, 1, 2, 3], [1, 2, 3, 4, 5].copyWithin(2, 0));
})();

// PACKED_DOUBLE_ELEMENTS.
(function packedDouble() {
  assertArrayEquals(
      [4.5, 5.5, 3.5, 4.5, 5.5], [1.5, 2.5, 3.5, 4.5, 5.5].copyWithin(0, 3));
  assertArrayEquals(
      [1.5, 2.5, 1.5, 2.5, 3.5], [1.5, 2.5, 3.5, 4.5, 5.5].copyWithin(2, 0));
})();

// PACKED_ELEMENTS (objects): values are copied by reference and the write
// barrier path is exercised.
(function packedObjects() {
  var a = {a: 1}, b = {b: 2}, c = {c: 3}, d = {d: 4}, e = {e: 5};
  var arr = [a, b, c, d, e];
  arr.copyWithin(0, 3);
  assertArrayEquals([d, e, c, d, e], arr);
  assertSame(d, arr[0]);
  assertSame(e, arr[1]);
})();

// HOLEY_SMI_ELEMENTS: holes move like absent elements.
(function holeySmi() {
  var arr = [0, , 2, , 4, ];
  arr.copyWithin(0, 3);
  assertArrayEquals([, 4, 2, , 4, ], arr);
  assertFalse(arr.hasOwnProperty(0));
  assertTrue(arr.hasOwnProperty(1));
})();

// HOLEY_DOUBLE_ELEMENTS.
(function holeyDouble() {
  var arr = [0.5, , 2.5, , 4.5, ];
  arr.copyWithin(0, 3);
  assertArrayEquals([, 4.5, 2.5, , 4.5, ], arr);
  assertFalse(arr.hasOwnProperty(0));
})();

// HOLEY_ELEMENTS (objects with holes).
(function holeyObjects() {
  var x = {x: 1}, y = {y: 2};
  var arr = [x, , y, , x, ];
  arr.copyWithin(0, 3);
  assertFalse(arr.hasOwnProperty(0));
  assertSame(x, arr[1]);
})();

// Copy-on-write arrays: the literal backing store must be copied before the
// in-place move, and other arrays sharing the COW store stay intact.
(function copyOnWrite() {
  function make() { return [1, 2, 3, 4, 5]; }
  var a = make();
  a.copyWithin(0, 3);
  assertArrayEquals([4, 5, 3, 4, 5], a);
  // A freshly created literal (which would share the COW store) is unaffected.
  assertArrayEquals([1, 2, 3, 4, 5], make());
})();

// count <= 0 and no-op ranges return the receiver unchanged.
(function emptyAndNoop() {
  var arr = [1, 2, 3, 4, 5];
  assertSame(arr, arr.copyWithin(2, 2));
  assertArrayEquals([1, 2, 3, 4, 5], arr);
  assertArrayEquals([], [].copyWithin(0, 3));
})();

// valueOf side effects that shrink the array must be re-validated: the fast
// path bails and the generic loop produces the spec result without OOB access.
(function valueOfShrinks() {
  var arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  var evil = {valueOf: function() { arr.length = 3; return 5; }};
  // length is read (10) before `start` triggers the shrink to 3.
  arr.copyWithin(0, evil);
  // No crash / OOB; the array length stays as the shrunk value.
  assertEquals(3, arr.length);
})();

// valueOf that changes the elements kind (smi -> double) before the copy.
(function valueOfChangesKind() {
  var arr = [1, 2, 3, 4, 5];
  var evil = {valueOf: function() { arr[0] = 1.5; return 3; }};
  arr.copyWithin(0, evil);
  assertArrayEquals([4, 5, 3, 4, 5], arr);
})();

// Prototype tampering invalidates the NoElementsProtector: holey arrays must
// fall back to the generic loop so inherited values are observed.
(function prototypeInheritedHole() {
  Array.prototype[3] = 'INHERITED';
  try {
    assertArrayEquals(
        ['INHERITED', 5, 3, 'INHERITED', 5], [1, 2, 3, , 5].copyWithin(0, 3));
  } finally {
    delete Array.prototype[3];
  }
})();

// Frozen / sealed arrays are not fast-copyable and must throw via the strict
// [[Set]] / [[Delete]] of the generic loop.
(function frozenThrows() {
  assertThrows(function() {
    return Object.freeze([1, 2, 3, 4, 5]).copyWithin(0, 3);
  }, TypeError);
})();

// Large arrays go through the same memmove path.
(function largeArray() {
  var n = 100000;
  var arr = new Array(n);
  for (var i = 0; i < n; i++) arr[i] = i;
  arr.copyWithin(1, 0, n - 1);
  assertEquals(0, arr[0]);
  for (var i = 1; i < n; i++) assertEquals(i - 1, arr[i]);
})();

// Optimized (Maglev/Turbofan) call sites must agree with the interpreter.
(function optimized() {
  function f(a) { return a.copyWithin(0, 3); }
  %PrepareFunctionForOptimization(f);
  assertArrayEquals([4, 5, 3, 4, 5], f([1, 2, 3, 4, 5]));
  assertArrayEquals([4, 5, 3, 4, 5], f([1, 2, 3, 4, 5]));
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals([4, 5, 3, 4, 5], f([1, 2, 3, 4, 5]));
  assertArrayEquals(
      [4.5, 5.5, 3.5, 4.5, 5.5], f([1.5, 2.5, 3.5, 4.5, 5.5]));
})();
