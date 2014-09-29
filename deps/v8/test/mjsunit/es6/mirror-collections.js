// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --expose-gc

function testMapMirror(mirror) {
  // Create JSON representation.
  var serializer = debug.MakeMirrorSerializer();
  var json = JSON.stringify(serializer.serializeValue(mirror));

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.ObjectMirror);
  assertTrue(mirror instanceof debug.MapMirror);

  assertTrue(mirror.isMap());

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('map', fromJSON.type);
}

function testSetMirror(mirror) {
  // Create JSON representation.
  var serializer = debug.MakeMirrorSerializer();
  var json = JSON.stringify(serializer.serializeValue(mirror));

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.ObjectMirror);
  assertTrue(mirror instanceof debug.SetMirror);

  assertTrue(mirror.isSet());

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('set', fromJSON.type);
}

var o1 = new Object();
var o2 = new Object();
var o3 = new Object();

// Test the mirror object for Maps
var map = new Map();
map.set(o1, 11);
map.set(o2, 22);
map.delete(o1);
var mapMirror = debug.MakeMirror(map);
testMapMirror(mapMirror);
var entries = mapMirror.entries();
assertEquals(1, entries.length);
assertSame(o2, entries[0].key);
assertEquals(22, entries[0].value);
map.set(o1, 33);
map.set(o3, o2);
map.delete(o2);
map.set(undefined, 44);
entries = mapMirror.entries();
assertEquals(3, entries.length);
assertSame(o1, entries[0].key);
assertEquals(33, entries[0].value);
assertSame(o3, entries[1].key);
assertSame(o2, entries[1].value);
assertEquals(undefined, entries[2].key);
assertEquals(44, entries[2].value);

// Test the mirror object for Sets
var set = new Set();
set.add(o1);
set.add(o2);
set.delete(o1);
set.add(undefined);
var setMirror = debug.MakeMirror(set);
testSetMirror(setMirror);
var values = setMirror.values();
assertEquals(2, values.length);
assertSame(o2, values[0]);
assertEquals(undefined, values[1]);

// Test the mirror object for WeakMaps
var weakMap = new WeakMap();
weakMap.set(o1, 11);
weakMap.set(new Object(), 22);
weakMap.set(o3, 33);
weakMap.set(new Object(), 44);
var weakMapMirror = debug.MakeMirror(weakMap);
testMapMirror(weakMapMirror);
weakMap.set(new Object(), 55);
assertTrue(weakMapMirror.entries().length <= 5);
gc();

function testWeakMapEntries(weakMapMirror) {
  var entries = weakMapMirror.entries();
  assertEquals(2, entries.length);
  var found = 0;
  for (var i = 0; i < entries.length; i++) {
    if (Object.is(entries[i].key, o1)) {
      assertEquals(11, entries[i].value);
      found++;
    }
    if (Object.is(entries[i].key, o3)) {
      assertEquals(33, entries[i].value);
      found++;
    }
  }
  assertEquals(2, found);
}

testWeakMapEntries(weakMapMirror);

// Test the mirror object for WeakSets
var weakSet = new WeakSet();
weakSet.add(o1);
weakSet.add(new Object());
weakSet.add(o2);
weakSet.add(new Object());
weakSet.add(new Object());
weakSet.add(o3);
weakSet.delete(o2);
var weakSetMirror = debug.MakeMirror(weakSet);
testSetMirror(weakSetMirror);
assertTrue(weakSetMirror.values().length <= 5);
gc();

function testWeakSetValues(weakSetMirror) {
  var values = weakSetMirror.values();
  assertEquals(2, values.length);
  var found = 0;
  for (var i = 0; i < values.length; i++) {
    if (Object.is(values[i], o1)) {
      found++;
    }
    if (Object.is(values[i], o3)) {
      found++;
    }
  }
  assertEquals(2, found);
}

testWeakSetValues(weakSetMirror);
