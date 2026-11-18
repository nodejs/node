// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-externalize-string

function readLastCodePoint(s) {
  return s.codePointAt(s.length - 1);
}

function readCodePointAt(s, i) {
  return s.codePointAt(i);
}

// Sliced SeqTwoByteString ending in a lead surrogate followed by an excluded
// trail surrogate in the parent string.
(function TestSlicedSeqTwoByteStringBoundary() {
  const prefix = 'Q'.repeat(40) + '\uD800';
  const a = (prefix + '\uDC00' + 'A'.repeat(40)).slice(25, 41);
  const b = (prefix + '\uDFFF' + 'A'.repeat(40)).slice(25, 41);
  assertEquals(a, b);

  %PrepareFunctionForOptimization(readLastCodePoint);
  assertEquals(0xD800, readLastCodePoint(a));
  assertEquals(0xD800, readLastCodePoint(b));
  %OptimizeMaglevOnNextCall(readLastCodePoint);
  assertEquals(0xD800, readLastCodePoint(a));
  assertEquals(0xD800, readLastCodePoint(b));
})();

// Valid surrogate pair strictly inside a SlicedString must still combine.
(function TestSlicedSeqTwoByteStringValidPair() {
  const s = ('Q'.repeat(40) + '\uD800\uDC00' + 'A'.repeat(40)).slice(25, 42);

  %PrepareFunctionForOptimization(readCodePointAt);
  assertEquals(0x10000, readCodePointAt(s, 15));
  assertEquals(0xDC00, readCodePointAt(s, 16));
  %OptimizeMaglevOnNextCall(readCodePointAt);
  assertEquals(0x10000, readCodePointAt(s, 15));
  assertEquals(0xDC00, readCodePointAt(s, 16));
})();

// Sliced ExternalTwoByteString ending in a lead surrogate (deferred runtime
// path). Externalize before slicing so the SlicedString parent is actually an
// ExternalTwoByteString rather than a flattened SeqTwoByteString copy.
(function TestSlicedExternalTwoByteStringBoundary() {
  const parentA = createExternalizableTwoByteString(
      %FlattenString('Q'.repeat(40) + '\uD800\uDC00' + 'A'.repeat(40)));
  const parentB = createExternalizableTwoByteString(
      %FlattenString('Q'.repeat(40) + '\uD800\uDFFF' + 'A'.repeat(40)));
  const parentPair = createExternalizableTwoByteString(
      %FlattenString('Q'.repeat(40) + '\uD800\uDC01' + 'A'.repeat(40)));
  externalizeString(parentA);
  externalizeString(parentB);
  externalizeString(parentPair);
  const a = parentA.slice(25, 41);
  const b = parentB.slice(25, 41);
  const pair = parentPair.slice(25, 42);

  function readExternal(s, i) {
    return s.codePointAt(i);
  }

  %PrepareFunctionForOptimization(readExternal);
  assertEquals(0xD800, readExternal(a, a.length - 1));
  assertEquals(0xD800, readExternal(b, b.length - 1));
  assertEquals(0x10001, readExternal(pair, 15));
  %OptimizeMaglevOnNextCall(readExternal);
  assertEquals(0xD800, readExternal(a, a.length - 1));
  assertEquals(0xD800, readExternal(b, b.length - 1));
  assertEquals(0x10001, readExternal(pair, 15));
})();

// One-byte SlicedString skips the boundary runtime bailout and unwraps inline.
(function TestSlicedOneByteStringBoundary() {
  const s = ('Q'.repeat(40) + 'Z' + 'A'.repeat(40)).slice(25, 41);

  function readLastOneByte(str) {
    return str.codePointAt(str.length - 1);
  }

  %PrepareFunctionForOptimization(readLastOneByte);
  assertEquals(0x5A, readLastOneByte(s));
  %OptimizeMaglevOnNextCall(readLastOneByte);
  assertEquals(0x5A, readLastOneByte(s));
})();
