// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints --sandbox-testing --expose-gc

// Reject left-trimmed backing store filler objects passed directly.
assertThrows(() => {
  let arr = new Array(200).fill(1);
  let elements = Sandbox.dereferenceTaggedPointerField(arr, 'elements');
  arr.shift();
  Sandbox.markForCorruptionOnAccess(elements, 0);
}, TypeError, 'First argument must not be a free-space or filler object');

// Reject objects that are left-trimmed during field name conversion.
assertThrows(() => {
  let arr = new Array(200).fill(1);
  let elements = Sandbox.dereferenceTaggedPointerField(arr, 'elements');
  let field = {
    toString() {
      arr.shift();
      return 'length';
    }
  };
  Sandbox.markForCorruptionOnAccess(elements, field);
}, TypeError, 'First argument must not be a free-space or filler object');

// Reject objects that are left-trimmed and cleared by GC during field name
// conversion.
assertThrows(() => {
  let arr = new Array(200).fill(1);
  let elements = Sandbox.dereferenceTaggedPointerField(arr, 'elements');
  let field = {
    toString() {
      arr.shift();
      gc();
      return 'length';
    }
  };
  Sandbox.markForCorruptionOnAccess(elements, field);
}, TypeError, 'First argument must be an object');

// Reject ThinStrings that are shortcut to read-only space during GC promotion.
assertThrows(() => {
  let a = 'toLocale';
  let b = 'String';
  let s = a + b;
  ({[s]: 1});
  Sandbox.markForCorruptionOnAccess(s, 0);
}, TypeError, 'First argument must not be a read-only object');
