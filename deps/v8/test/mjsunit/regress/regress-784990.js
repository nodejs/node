// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const key1 = {};
const key2 = {};

const set = new Set([, 1]);
assertEquals(set.has(undefined), true);
assertEquals(set.has(1), true);

const doubleSet = new Set([,1.234]);
assertEquals(doubleSet.has(undefined), true);
assertEquals(doubleSet.has(1.234), true);

const map = new Map([[, key1], [key2, ]]);
assertEquals(map.get(undefined), key1);
assertEquals(map.get(key2), undefined);

const doublesMap = new Map([[, 1.234]]);
assertEquals(doublesMap.get(undefined), 1.234);

const weakmap = new WeakMap([[key1, ]]);
assertEquals(weakmap.get(key1), undefined);

assertThrows(() => new WeakSet([, {}]));
assertThrows(() => new WeakSet([, 1.234]));
assertThrows(() => new Map([, [, key1]]));
assertThrows(() => new WeakMap([[, key1]]));
assertThrows(() => new WeakMap([, [, key1]]));
