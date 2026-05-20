// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This tests the interaction between the MapIterator protector and SetIterator
// protector.

var map = new Map([[1,2], [2,3], [3,4]]);
assertTrue(%MapIteratorProtector());

var set = new Set([1,2,3]);
assertTrue(%SetIteratorProtector());

// This changes %MapIteratorPrototype%. No more tests should be run after this
// in the same instance.
var iterator = map.keys();
iterator.__proto__[Symbol.iterator] = () => ({next: () => ({done: true})});

assertFalse(%MapIteratorProtector());
assertEquals([[1,2], [2,3], [3,4]], [...map]);
assertEquals([], [...map.entries()]);
assertEquals([], [...map.keys()]);
assertEquals([], [...map.values()]);
assertEquals([], [...iterator]);

assertTrue(%SetIteratorProtector());
assertEquals([1,2,3], [...set]);
assertEquals([[1,1],[2,2],[3,3]], [...set.entries()]);
assertEquals([1,2,3], [...set.keys()]);
assertEquals([1,2,3], [...set.values()]);
