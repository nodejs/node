// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs-with-cleanup-some

(function TestCleanupSomeWithoutFinalizationRegistry() {
  assertThrows(() => FinalizationRegistry.prototype.cleanupSome.call({}), TypeError);
  // Does not throw:
  let fg = new FinalizationRegistry(() => {});
  let rv = FinalizationRegistry.prototype.cleanupSome.call(fg);
  assertEquals(undefined, rv);
})();

(function TestCleanupSomeWithNonCallableCallback() {
  let fg = new FinalizationRegistry(() => {});
  assertThrows(() => fg.cleanupSome(1), TypeError);
  assertThrows(() => fg.cleanupSome(1n), TypeError);
  assertThrows(() => fg.cleanupSome(Symbol()), TypeError);
  assertThrows(() => fg.cleanupSome({}), TypeError);
  assertThrows(() => fg.cleanupSome('foo'), TypeError);
  assertThrows(() => fg.cleanupSome(true), TypeError);
  assertThrows(() => fg.cleanupSome(false), TypeError);
  assertThrows(() => fg.cleanupSome(null), TypeError);
})();
