// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan

// Compares against read-only roots, which arm64 reads as immediates:
// the FixedArray map that a keyed store checks its elements against, and the
// read-only "length" string that a dictionary-mode load checks its entry's
// key against.

// A keyed store that has only seen writable elements checks them against the
// FixedArray map and deoptimizes on a copy-on-write array.
function store(a, i, v) {
  a[i] = v;
  return a;
}
function writable() {
  const a = [];
  a.push(1, 2, 3);
  return a;
}
%PrepareFunctionForOptimization(store);
assertEquals([1, 7, 3], store(writable(), 1, 7));
assertEquals([1, 2, 8], store(writable(), 2, 8));
%OptimizeMaglevOnNextCall(store);
assertEquals([9, 2, 3], store(writable(), 0, 9));
assertMaglevved(store);
// The literal's elements are copy-on-write.
assertEquals([1, 2, 10], store([1, 2, 3], 2, 10));
assertUnoptimized(store);

// A dictionary-mode load of "length" compares the entry's key against the
// read-only string.
function length(o) {
  return o.length;
}
function makeDictionary() {
  const o = {};
  for (let i = 0; i < 150; i++) o["p" + i] = i;
  o.length = 42;
  return o;
}
const dict = makeDictionary();
%PrepareFunctionForOptimization(length);
assertEquals(42, length(dict));
assertEquals(42, length(dict));
%OptimizeMaglevOnNextCall(length);
assertEquals(42, length(dict));
assertMaglevved(length);
dict.length = 43;
assertEquals(43, length(dict));
delete dict.length;
assertEquals(undefined, length(dict));
assertMaglevved(length);
