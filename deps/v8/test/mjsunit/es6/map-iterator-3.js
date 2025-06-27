// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var map = new Map([[1,2], [2,3], [3,4]]);
assertTrue(%MapIteratorProtector());
assertTrue(%SetIteratorProtector());

// This changes %MapIteratorPrototype%. No more tests should be run after this
// in the same instance.
var iterator = map[Symbol.iterator]();
iterator.__proto__.next = () => ({done: true});

assertFalse(%MapIteratorProtector());
assertTrue(%SetIteratorProtector());
assertEquals([], [...map]);
assertEquals([], [...map.entries()]);
assertEquals([], [...map.keys()]);
assertEquals([], [...map.values()]);
assertEquals([], [...iterator]);
