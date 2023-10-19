// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var map = new Map([[1,2], [2,3], [3,4]]);

assertEquals([[1,2], [2,3], [3,4]], [...map]);
assertEquals([[1,2], [2,3], [3,4]], [...map.entries()]);
assertEquals([1,2,3], [...map.keys()]);
assertEquals([2,3,4], [...map.values()]);
assertTrue(%MapIteratorProtector());
assertTrue(%SetIteratorProtector());

map[Symbol.iterator] = () => ({next: () => ({done: true})});

assertTrue(%MapIteratorProtector());
assertTrue(%SetIteratorProtector());
assertEquals([], [...map]);
assertEquals([[1,2], [2,3], [3,4]], [...map.entries()]);
assertEquals([1,2,3], [...map.keys()]);
assertEquals([2,3,4], [...map.values()]);
