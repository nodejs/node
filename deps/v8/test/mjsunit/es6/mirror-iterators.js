// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug
// Test the mirror object for collection iterators.

function testIteratorMirror(iter, offset, expected) {
  while (offset-- > 0) iter.next();

  var mirror = debug.MakeMirror(iter);
  assertTrue(mirror.isIterator());

  var preview = mirror.preview();
  assertArrayEquals(expected, preview);

  // Check that iterator has not changed after taking preview.
  var values = [];
  for (var i of iter) values.push(i);
  assertArrayEquals(expected, values);
}

var o1 = { foo: 1 };
var o2 = { foo: 2 };

var map = new Map();
map.set(41, 42);
map.set(o1, o2);

testIteratorMirror(map.keys(), 0, [41, o1]);
testIteratorMirror(map.values(), 0, [42, o2]);
testIteratorMirror(map.entries(), 0, [[41, 42], [o1, o2]]);

testIteratorMirror(map.keys(), 1, [o1]);
testIteratorMirror(map.values(), 1, [o2]);
testIteratorMirror(map.entries(), 1, [[o1, o2]]);

testIteratorMirror(map.keys(), 2, []);
testIteratorMirror(map.values(), 2, []);
testIteratorMirror(map.entries(), 2, []);

var set = new Set();
set.add(41);
set.add(42);
set.add(o1);
set.add(o2);

testIteratorMirror(set.keys(), 0, [41, 42, o1, o2]);
testIteratorMirror(set.values(), 0, [41, 42, o1, o2]);
testIteratorMirror(set.entries(), 0, [[41, 41], [42, 42], [o1, o1], [o2, o2]]);

testIteratorMirror(set.keys(), 1, [42, o1, o2]);
testIteratorMirror(set.values(), 1, [42, o1, o2]);
testIteratorMirror(set.entries(), 1, [[42, 42], [o1, o1], [o2, o2]]);

testIteratorMirror(set.keys(), 3, [o2]);
testIteratorMirror(set.values(), 3, [o2]);
testIteratorMirror(set.entries(), 3, [[o2, o2]]);

testIteratorMirror(set.keys(), 5, []);
testIteratorMirror(set.values(), 5, []);
testIteratorMirror(set.entries(), 5, []);
