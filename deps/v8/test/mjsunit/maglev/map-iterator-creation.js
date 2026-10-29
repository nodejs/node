// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function getEntries(m) {
  return m.entries();
}

function getKeys(m) {
  return m.keys();
}

function getValues(m) {
  return m.values();
}

const map = new Map([[1, 10], [2, 20]]);

%PrepareFunctionForOptimization(getEntries);
let it1 = getEntries(map);
assertEquals([1, 10], it1.next().value);
%OptimizeMaglevOnNextCall(getEntries);
let it2 = getEntries(map);
assertEquals([1, 10], it2.next().value);
assertEquals([2, 20], it2.next().value);
assertTrue(it2.next().done);

%PrepareFunctionForOptimization(getKeys);
let kit1 = getKeys(map);
assertEquals(1, kit1.next().value);
%OptimizeMaglevOnNextCall(getKeys);
let kit2 = getKeys(map);
assertEquals(1, kit2.next().value);
assertEquals(2, kit2.next().value);
assertTrue(kit2.next().done);

%PrepareFunctionForOptimization(getValues);
let vit1 = getValues(map);
assertEquals(10, vit1.next().value);
%OptimizeMaglevOnNextCall(getValues);
let vit2 = getValues(map);
assertEquals(10, vit2.next().value);
assertEquals(20, vit2.next().value);
assertTrue(vit2.next().done);
