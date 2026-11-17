// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const receiver = {prefix_for_key_check_long_enough: 0x1234};

function testMapHas(obj, map, suffix) {
  const key = 'prefix_for_key_check_long_enough' + suffix;
  const has = map.has(key);
  return [obj[key], has];
}

function testMapGet(obj, map, suffix) {
  const key = 'prefix_for_key_check_long_enough' + suffix;
  const val = map.get(key);
  return [obj[key], val];
}

function testSetHas(obj, set, suffix) {
  const key = 'prefix_for_key_check_long_enough' + suffix;
  const has = set.has(key);
  return [obj[key], has];
}

// Map.prototype.has
{
  const map = new Map([['prefix_for_key_check_long_enough_hit', 42]]);
  %PrepareFunctionForOptimization(testMapHas);
  assertEquals([0x1234, false], testMapHas(receiver, map, ''));
  assertEquals([0x1234, false], testMapHas(receiver, map, ''));
  %OptimizeFunctionOnNextCall(testMapHas);
  assertEquals([undefined, false], testMapHas(receiver, map, '_miss'));
  assertEquals([undefined, true], testMapHas(receiver, map, '_hit'));
}

// Map.prototype.get
{
  const map = new Map([['prefix_for_key_check_long_enough_hit', 42]]);
  %PrepareFunctionForOptimization(testMapGet);
  assertEquals([0x1234, undefined], testMapGet(receiver, map, ''));
  assertEquals([0x1234, undefined], testMapGet(receiver, map, ''));
  %OptimizeFunctionOnNextCall(testMapGet);
  assertEquals([undefined, undefined], testMapGet(receiver, map, '_miss'));
  assertEquals([undefined, 42], testMapGet(receiver, map, '_hit'));
}

// Set.prototype.has
{
  const set = new Set(['prefix_for_key_check_long_enough_hit']);
  %PrepareFunctionForOptimization(testSetHas);
  assertEquals([0x1234, false], testSetHas(receiver, set, ''));
  assertEquals([0x1234, false], testSetHas(receiver, set, ''));
  %OptimizeFunctionOnNextCall(testSetHas);
  assertEquals([undefined, false], testSetHas(receiver, set, '_miss'));
  assertEquals([undefined, true], testSetHas(receiver, set, '_hit'));
}
