// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug
// Test the mirror object for functions.

function *generator(f) {
  "use strict";
  yield;
  f();
  yield;
}

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

function TestGeneratorMirror(g, status, line, column, receiver) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(g);
  var serializer = debug.MakeMirrorSerializer();
  var json = JSON.stringify(serializer.serializeValue(mirror));
  var refs = new MirrorRefCache(
      JSON.stringify(serializer.serializeReferencedObjects()));

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.ObjectMirror);
  assertTrue(mirror instanceof debug.GeneratorMirror);

  // Check the mirror properties.
  assertTrue(mirror.isGenerator());
  assertEquals('generator', mirror.type());
  assertFalse(mirror.isPrimitive());
  assertEquals('Generator', mirror.className());

  assertEquals(receiver, mirror.receiver().value());
  assertEquals(generator, mirror.func().value());

  assertEquals(status, mirror.status());

  // Note that line numbers are 0-based, not 1-based.
  var loc = mirror.sourceLocation();
  if (status === 'suspended') {
    assertTrue(!!loc);
    assertEquals(line, loc.line);
    assertEquals(column, loc.column);
  } else {
    assertEquals(undefined, loc);
  }

  TestInternalProperties(mirror, status, receiver);
}

function TestInternalProperties(mirror, status, receiver) {
  var properties = mirror.internalProperties();
  assertEquals(3, properties.length);
  assertEquals("[[GeneratorStatus]]", properties[0].name());
  assertEquals(status, properties[0].value().value());
  assertEquals("[[GeneratorFunction]]", properties[1].name());
  assertEquals(generator, properties[1].value().value());
  assertEquals("[[GeneratorReceiver]]", properties[2].name());
  assertEquals(receiver, properties[2].value().value());
}

var iter = generator(function() {
  var mirror = debug.MakeMirror(iter);
  assertEquals('running', mirror.status());
  assertEquals(undefined, mirror.sourceLocation());
  TestInternalProperties(mirror, 'running');
});

TestGeneratorMirror(iter, 'suspended', 7, 19);

iter.next();
TestGeneratorMirror(iter, 'suspended', 9, 2);

iter.next();
TestGeneratorMirror(iter, 'suspended', 11, 2);

iter.next();
TestGeneratorMirror(iter, 'closed');

// Test generator with receiver.
var obj = {foo: 42};
var iter2 = generator.call(obj, function() {
  var mirror = debug.MakeMirror(iter2);
  assertEquals('running', mirror.status());
  assertEquals(undefined, mirror.sourceLocation());
  TestInternalProperties(mirror, 'running', obj);
});

TestGeneratorMirror(iter2, 'suspended', 7, 19, obj);

iter2.next();
TestGeneratorMirror(iter2, 'suspended', 9, 2, obj);

iter2.next();
TestGeneratorMirror(iter2, 'suspended', 11, 2, obj);

iter2.next();
TestGeneratorMirror(iter2, 'closed', 0, 0, obj);
