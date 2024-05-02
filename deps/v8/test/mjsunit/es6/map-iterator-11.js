// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var map = new Map([[1,2], [2,3], [3,4]]);

var iterator = map.keys();
assertEquals([1,2,3], [...map.keys()]);
assertEquals([1,2,3], [...iterator]);
assertEquals([], [...iterator]);

iterator = map.values();
assertEquals([2,3,4], [...iterator]);
assertEquals([], [...iterator]);

iterator = map.keys();
iterator.next();
assertEquals([2,3], [...iterator]);
assertEquals([], [...iterator]);

iterator = map.values();
var iterator2 = map.values();

map.delete(1);
assertEquals([3,4], [...iterator]);
assertEquals([], [...iterator]);

map.set(1,5);
assertEquals([3,4,5], [...iterator2]);
assertEquals([], [...iterator2]);

iterator = map.keys();
map.set(4,6);
assertEquals([2,3,1,4], [...iterator]);
assertEquals([], [...iterator]);
