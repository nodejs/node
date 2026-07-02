// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-sum-precise

(function TestArrayIteratorClearedOnError() {
  const it = [10, 20, "bad", 30, 40][Symbol.iterator]();
  assertThrows(() => Math.sumPrecise(it), TypeError);
  assertEquals({value: undefined, done: true}, it.next());

  const it2 = [10, 20, "bad", 30, 40][Symbol.iterator]();
  assertThrows(() => Math.sumPrecise(it2), TypeError);
  assertEquals(-0, Math.sumPrecise(it2));
})();

(function TestMapIteratorClearedOnError() {
  let m = new Map([[1, 10], [2, "bad"], [3, 30]]);
  let it = m.values();
  assertThrows(() => Math.sumPrecise(it), TypeError);
  assertEquals({value: undefined, done: true}, it.next());

  let it2 = m.values();
  assertThrows(() => Math.sumPrecise(it2), TypeError);
  assertEquals(-0, Math.sumPrecise(it2));
})();
