// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Per ECMA-262 IteratorClose, built-in Set iterators lack a .return() method
// and are not exhausted on abrupt completion (crbug.com/573855950).

(function TestSetIteratorNotExhaustedOnError() {
  let s = new Set([1, "a", 3]);
  let it = s[Symbol.iterator]();

  assertThrows(() => Math.sumPrecise(it), TypeError);

  assertEquals({value: 3, done: false}, it.next());
  assertEquals({value: undefined, done: true}, it.next());
})();

(function TestSetIteratorNotExhaustedOnErrorAndSetChanges() {
  let s = new Set([1, "a", 3]);
  let it = s[Symbol.iterator]();

  assertThrows(() => Math.sumPrecise(it), TypeError);

  s.add(4);
  assertEquals(7, Math.sumPrecise(it));
  assertEquals({value: undefined, done: true}, it.next());
})();
