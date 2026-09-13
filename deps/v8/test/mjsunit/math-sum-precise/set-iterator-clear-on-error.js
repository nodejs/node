// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-sum-precise

(function TestSetIteratorNotClearedOnError() {
  let s = new Set([1, "a", 3]);
  let it = s[Symbol.iterator]();

  assertThrows(() => Math.sumPrecise(it), TypeError);

  let next = it.next();
  assertTrue(next.done);
  assertEquals(undefined, next.value);
})();

(function TestSetIteratorNotClearedOnErrorAndSetChanges() {
  let s = new Set([1, "a", 3]);
  let it = s[Symbol.iterator]();

  assertThrows(() => Math.sumPrecise(it), TypeError);

  s.add(4);
  let next = it.next();
  assertTrue(next.done);
  assertEquals(undefined, next.value);
})();
