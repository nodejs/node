// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug
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
  assertTrue(mirror.promiseValue() instanceof debug.Mirror);
  assertEquals(value, mirror.promiseValue().value());

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('promise', fromJSON.type);
  assertEquals('Object', fromJSON.className);
  assertEquals('function', refs.lookup(fromJSON.constructorFunction.ref).type);
  assertEquals('Promise', refs.lookup(fromJSON.constructorFunction.ref).name);
  assertEquals(status, fromJSON.status);
  assertEquals(value, refs.lookup(fromJSON.promiseValue.ref).value);
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

// Test internal properties of different promises.
var m1 = debug.MakeMirror(new Promise(
    function(resolve, reject) { resolve(1) }));
var ip = m1.internalProperties();
assertEquals(2, ip.length);
assertEquals("[[PromiseStatus]]", ip[0].name());
assertEquals("[[PromiseValue]]", ip[1].name());
assertEquals("resolved", ip[0].value().value());
assertEquals(1, ip[1].value().value());

var m2 = debug.MakeMirror(new Promise(function(resolve, reject) { reject(2) }));
ip = m2.internalProperties();
assertEquals("rejected", ip[0].value().value());
assertEquals(2, ip[1].value().value());

var m3 = debug.MakeMirror(new Promise(function(resolve, reject) { }));
ip = m3.internalProperties();
assertEquals("pending", ip[0].value().value());
assertEquals("undefined", typeof(ip[1].value().value()));
