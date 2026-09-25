// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let ar0 = Array(0xabcdef).fill(7);

(function TestFastArrayConcat() {
  let ar = [153];
  assertFalse(%HasDictionaryElements(ar));

  let args = Array(12).fill(ar0);
  assertThrows(() => ar.concat(...args), RangeError, /Invalid array length/);
})();

(function TestSlowArrayConcat() {
  let ar = [];
  Object.defineProperty(ar, 0, {value:42, configurable: true});
  assertTrue(%HasDictionaryElements(ar));

  let args = Array(12).fill(ar0);
  assertThrows(() => ar.concat(...args), RangeError, /Invalid array length/);
})();
