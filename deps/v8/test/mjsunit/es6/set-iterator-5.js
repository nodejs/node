// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-opt

var set = new Set([1,2,3]);

assertTrue(%SetIteratorProtector());
assertTrue(%MapIteratorProtector());

// This changes %SetIteratorPrototype%. No more tests should be run after this
// in the same instance.
var iterator = set.values();
iterator.__proto__.next = () => ({done: true});

assertFalse(%SetIteratorProtector());
assertTrue(%MapIteratorProtector());
assertEquals([], [...set]);
assertEquals([], [...set.entries()]);
assertEquals([], [...set.keys()]);
assertEquals([], [...set.values()]);
assertEquals([], [...iterator]);
