// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Per ECMA-262 IteratorClose, built-in iterators lack a .return() method and
// must remain at the element immediately following the failing element on
// abrupt completion (crbug.com/573855950).

(function TestArrayIteratorNotExhaustedOnError() {
  const it = [10, 20, "bad", 30, 40][Symbol.iterator]();
  assertThrows(() => Math.sumPrecise(it), TypeError);
  assertEquals({value: 30, done: false}, it.next());

  const it2 = [10, 20, "bad", 30, 40][Symbol.iterator]();
  assertThrows(() => Math.sumPrecise(it2), TypeError);
  assertEquals(70, Math.sumPrecise(it2));
})();

(function TestMapIteratorNotExhaustedOnError() {
  let m = new Map([[1, 10], [2, "bad"], [3, 30]]);
  let it = m.values();
  assertThrows(() => Math.sumPrecise(it), TypeError);
  assertEquals({value: 30, done: false}, it.next());

  let it2 = m.values();
  assertThrows(() => Math.sumPrecise(it2), TypeError);
  assertEquals(30, Math.sumPrecise(it2));
})();

(function TestStringIteratorNotExhaustedOnError() {
  let it = "abc"[Symbol.iterator]();
  assertThrows(() => Math.sumPrecise(it), TypeError);
  assertEquals({value: "b", done: false}, it.next());
})();

(function TestRegExpStringIteratorNotExhaustedOnError() {
  let it = "ab".matchAll(/./g);
  assertThrows(() => Math.sumPrecise(it), TypeError);
  let next = it.next();
  assertFalse(next.done);
  assertEquals("b", next.value[0]);
})();

(function TestEmptyArrayIteratorExhausted() {
  let a = [];
  let it = a.values();
  assertEquals(-0, Math.sumPrecise(it));
  a.push(1);
  assertEquals({value: undefined, done: true}, it.next());
})();
