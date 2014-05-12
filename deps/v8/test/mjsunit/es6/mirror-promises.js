// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --harmony-promises
// Test the mirror object for promises.

function MirrorRefCache(json_refs) {
  var tmp = eval('(' + json_refs + ')');
  this.refs_ = [];
  for (var i = 0; i < tmp.length; i++) {
    this.refs_[tmp[i].handle] = tmp[i];
  }
}

MirrorRefCache.prototype.lookup = function(handle) {
  return this.refs_[handle];
}

function testPromiseMirror(promise, status, value) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(promise);
  var serializer = debug.MakeMirrorSerializer();
  var json = JSON.stringify(serializer.serializeValue(mirror));
  var refs = new MirrorRefCache(
      JSON.stringify(serializer.serializeReferencedObjects()));

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.ObjectMirror);
  assertTrue(mirror instanceof debug.PromiseMirror);

  // Check the mirror properties.
  assertEquals(status, mirror.status());
  assertTrue(mirror.isPromise());
  assertEquals('promise', mirror.type());
  assertFalse(mirror.isPrimitive());
  assertEquals("Object", mirror.className());
  assertEquals("#<Promise>", mirror.toText());
  assertSame(promise, mirror.value());
  assertEquals(value, mirror.promiseValue());

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('promise', fromJSON.type);
  assertEquals('Object', fromJSON.className);
  assertEquals('function', refs.lookup(fromJSON.constructorFunction.ref).type);
  assertEquals('Promise', refs.lookup(fromJSON.constructorFunction.ref).name);
  assertEquals(status, fromJSON.status);
  assertEquals(value, fromJSON.promiseValue);
}

// Test a number of different promises.
var resolved = new Promise(function(resolve, reject) { resolve() });
var rejected = new Promise(function(resolve, reject) { reject() });
var pending = new Promise(function(resolve, reject) {});

testPromiseMirror(resolved, "resolved", undefined);
testPromiseMirror(rejected, "rejected", undefined);
testPromiseMirror(pending, "pending", undefined);

var resolvedv = new Promise(function(resolve, reject) { resolve('resolve') });
var rejectedv = new Promise(function(resolve, reject) { reject('reject') });
var thrownv = new Promise(function(resolve, reject) { throw 'throw' });

testPromiseMirror(resolvedv, "resolved", 'resolve');
testPromiseMirror(rejectedv, "rejected", 'reject');
testPromiseMirror(thrownv, "rejected", 'throw');
