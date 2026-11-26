// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev --turbofan

function mapHas(m, k) {
  return m.has(k);
}

function mapHasInt32(m, k) {
  return m.has(k | 0);
}

function mapHasLoop(m, n) {
  let count = 0;
  for (let i = 0; i < n; i++) {
    if (m.has(i)) count++;
  }
  return count;
}

const objKey = {};
const m = new Map([
  ['foo', 1],
  [1, 10],
  [42, 2],
  [-0, 3],
  [NaN, 4],
  [objKey, 5],
]);
m.delete(1);

%PrepareFunctionForOptimization(mapHas);
%PrepareFunctionForOptimization(mapHasInt32);
%PrepareFunctionForOptimization(mapHasLoop);
for (let i = 0; i < 10; i++) {
  assertTrue(mapHas(m, 'foo'));
  assertFalse(mapHas(m, 'bar'));
  assertTrue(mapHas(m, 0));
  assertTrue(mapHas(m, -0));
  assertTrue(mapHas(m, NaN));
  assertTrue(mapHas(m, objKey));
  assertFalse(mapHas(m, {}));

  assertFalse(mapHasInt32(m, 1));
  assertTrue(mapHasInt32(m, 42));
  assertFalse(mapHasInt32(m, 99));
  assertTrue(mapHasInt32(m, 0));

  assertEquals(2, mapHasLoop(m, 50));
}

%OptimizeFunctionOnNextCall(mapHas);
%OptimizeFunctionOnNextCall(mapHasInt32);
%OptimizeFunctionOnNextCall(mapHasLoop);

assertTrue(mapHas(m, 'foo'));
assertTrue(mapHas(m, 'fo' + 'o'));
assertFalse(mapHas(m, 'bar'));
assertTrue(mapHas(m, 0));
assertTrue(mapHas(m, -0));
assertTrue(mapHas(m, NaN));
assertTrue(mapHas(m, objKey));
assertFalse(mapHas(m, {}));
assertOptimized(mapHas);

assertFalse(mapHasInt32(m, 1));
assertTrue(mapHasInt32(m, 42));
assertFalse(mapHasInt32(m, 99));
assertTrue(mapHasInt32(m, 0));
assertOptimized(mapHasInt32);

assertEquals(2, mapHasLoop(m, 50));
assertOptimized(mapHasLoop);

// Non-Map receiver deopts cleanly.
const fakeMap = { has(k) { return k === 'fake'; } };
assertTrue(mapHas(fakeMap, 'fake'));
assertFalse(mapHas(fakeMap, 'other'));

// Missing arguments are undefined, extra arguments are ignored.
function argCounts(m, s) {
  return [m.has(), m.has(42, 'extra'), m.get(), m.get(42, 'extra'), s.has(),
          s.has(42, 'extra')];
}
const mu = new Map([[undefined, 7], [42, 8]]);
const su = new Set([undefined, 42]);
const expected = [true, true, 7, 8, true, true];
%PrepareFunctionForOptimization(argCounts);
assertEquals(expected, argCounts(mu, su));
assertEquals(expected, argCounts(mu, su));
%OptimizeFunctionOnNextCall(argCounts);
assertEquals(expected, argCounts(mu, su));
assertOptimized(argCounts);
assertEquals([false, false, undefined, undefined, false, false],
             argCounts(new Map(), new Set()));
