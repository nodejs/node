// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Array.prototype.indexOf on dictionary-elements receivers takes a fast path
// that iterates the backing NumberDictionary's entries instead of probing
// every index up to length. These tests pin down its observable semantics.

function makeDictionaryArray() {
  const a = [];
  a[0] = 'a';
  a[10] = 'b';
  a[1000] = 'c';
  a[500000] = 'b';
  a[1000000] = 'd';
  assertTrue(%HasDictionaryElements(a));
  return a;
}

// Returns the smallest matching index even though dictionary iteration
// order is arbitrary ('b' is present at 10 and 500000).
(function testSmallestIndexWins() {
  const a = makeDictionaryArray();
  assertEquals(10, a.indexOf('b'));
  assertEquals(0, a.indexOf('a'));
  assertEquals(1000000, a.indexOf('d'));
  assertEquals(-1, a.indexOf('z'));
})();

// fromIndex excludes entries below it.
(function testFromIndex() {
  const a = makeDictionaryArray();
  assertEquals(10, a.indexOf('b', 10));
  assertEquals(500000, a.indexOf('b', 11));
  assertEquals(-1, a.indexOf('a', 1));
  // Negative fromIndex is relative to length.
  assertEquals(1000000, a.indexOf('d', -1));
  assertEquals(-1, a.indexOf('a', -1));
})();

// Holes are skipped: searching undefined only matches explicit undefined.
(function testUndefinedSearch() {
  const a = makeDictionaryArray();
  assertEquals(-1, a.indexOf(undefined));
  a[700000] = undefined;
  assertTrue(%HasDictionaryElements(a));
  assertEquals(700000, a.indexOf(undefined));
})();

// Strict equality semantics: NaN is never found, +0 matches -0.
(function testStrictEquals() {
  const a = [];
  a[100000] = NaN;
  a[200000] = -0;
  assertTrue(%HasDictionaryElements(a));
  assertEquals(-1, a.indexOf(NaN));
  assertEquals(200000, a.indexOf(0));
})();

// An accessor element forces the ordered slow path: getters run in index
// order and their values are observed.
(function testAccessorBailout() {
  const a = [];
  a[100000] = 'x';
  const log = [];
  Object.defineProperty(a, 50000, {
    get() {
      log.push(50000);
      return 'y';
    },
  });
  Object.defineProperty(a, 60000, {
    get() {
      log.push(60000);
      return 'x';
    },
  });
  assertTrue(%HasDictionaryElements(a));
  assertEquals(60000, a.indexOf('x'));
  assertEquals([50000, 60000], log);
  assertEquals(50000, a.indexOf('y'));
})();
