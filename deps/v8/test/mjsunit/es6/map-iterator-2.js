// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-opt

var map = new Map([[1,2], [2,3], [3,4]]);
assertTrue(%MapIteratorProtector());
assertTrue(%SetIteratorProtector());

// This changes %MapPrototype%. No more tests should be run after this in the
// same instance.
map.__proto__[Symbol.iterator] = () => ({next: () => ({done: true})});

assertTrue(%MapIteratorProtector());
assertTrue(%SetIteratorProtector());
assertEquals([], [...map]);
assertEquals([[1,2], [2,3], [3,4]], [...map.entries()]);
assertEquals([1,2,3], [...map.keys()]);
assertEquals([2,3,4], [...map.values()]);
