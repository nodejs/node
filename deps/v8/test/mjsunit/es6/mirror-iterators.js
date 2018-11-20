// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug
// Test the mirror object for collection iterators.

function testIteratorMirror(iter, offset, expected, opt_limit) {
  while (offset-- > 0) iter.next();

  var mirror = debug.MakeMirror(iter);
  assertTrue(mirror.isIterator());

  var preview = mirror.preview(opt_limit);
  assertArrayEquals(expected, preview);

  // Check that iterator has not changed after taking preview.
  var values = [];
  for (var i of iter) {
    if (opt_limit && values.length >= opt_limit) break;
    values.push(i);
  }
  assertArrayEquals(expected, values);
}

function testIteratorInternalProperties(iter, offset, kind, index, has_more) {
  while (offset-- > 0) iter.next();

  var mirror = debug.MakeMirror(iter);
  assertTrue(mirror.isIterator());

  var properties = mirror.internalProperties();
  assertEquals(3, properties.length);
  assertEquals("[[IteratorHasMore]]", properties[0].name());
  assertEquals(has_more, properties[0].value().value());
  assertEquals("[[IteratorIndex]]", properties[1].name());
  assertEquals(index, properties[1].value().value());
  assertEquals("[[IteratorKind]]", properties[2].name());
  assertEquals(kind, properties[2].value().value());
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

// Test with maximum limit.
testIteratorMirror(map.keys(), 0, [41], 1);
testIteratorMirror(map.values(), 0, [42], 1);
testIteratorMirror(map.entries(), 0, [[41, 42]], 1);

testIteratorInternalProperties(map.keys(), 0, "keys", 0, true);
testIteratorInternalProperties(map.values(), 1, "values", 1, true);
testIteratorInternalProperties(map.entries(), 2, "entries", 2, false);
testIteratorInternalProperties(map.keys(), 3, "keys", 2, false);

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

// Test with maximum limit.
testIteratorMirror(set.keys(), 1, [42, o1], 2);
testIteratorMirror(set.values(), 1, [42, o1], 2);
testIteratorMirror(set.entries(), 1, [[42, 42], [o1, o1]], 2);

testIteratorInternalProperties(set.keys(), 0, "values", 0, true);
testIteratorInternalProperties(set.values(), 1, "values", 1, true);
testIteratorInternalProperties(set.entries(), 2, "entries", 2, true);
testIteratorInternalProperties(set.keys(), 3, "values", 3, true);
testIteratorInternalProperties(set.values(), 4, "values", 4, false);
testIteratorInternalProperties(set.entries(), 5, "entries", 4, false);
