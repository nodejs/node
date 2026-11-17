// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

const prefix = 'ABCDEFGHIJKLMNO';
const key = %ConstructInternalizedString(prefix + 'P');

function inspect(s, expected) {
  return [s === expected, s.length, s.charCodeAt(15)];
}
%PrepareFunctionForOptimization(inspect);
inspect(key, prefix);
%OptimizeFunctionOnNextCall(inspect);
inspect(key, prefix);

function testMapGet(map, suffix) {
  const s = prefix + ('P' + suffix);
  const found = map.get(s);
  return [found, ...inspect(s, prefix)];
}

function testSetHas(set, suffix) {
  const s = prefix + ('P' + suffix);
  const found = set.has(s);
  return [found, ...inspect(s, prefix)];
}

// Map.prototype.get
{
  const map = new Map([[key, 42]]);
  %PrepareFunctionForOptimization(testMapGet);
  assertEquals([42, false, 16, 80], testMapGet(map, ''));
  assertEquals([42, false, 16, 80], testMapGet(map, ''));
  %OptimizeFunctionOnNextCall(testMapGet);
  assertEquals([42, false, 16, 80], testMapGet(map, ''));
}

// Set.prototype.has
{
  const set = new Set([key]);
  %PrepareFunctionForOptimization(testSetHas);
  assertEquals([true, false, 16, 80], testSetHas(set, ''));
  assertEquals([true, false, 16, 80], testSetHas(set, ''));
  %OptimizeFunctionOnNextCall(testSetHas);
  assertEquals([true, false, 16, 80], testSetHas(set, ''));
}
