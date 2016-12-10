// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --harmony-async-await --allow-natives-syntax
// Test the mirror object for functions.

var AsyncFunction = (async function() {}).constructor;

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

function testFunctionMirror(f) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(f);
  var serializer = debug.MakeMirrorSerializer();
  var json = JSON.stringify(serializer.serializeValue(mirror));
  var refs = new MirrorRefCache(
      JSON.stringify(serializer.serializeReferencedObjects()));

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.ObjectMirror);
  assertTrue(mirror instanceof debug.FunctionMirror);

  // Check the mirror properties.
  assertTrue(mirror.isFunction());
  assertEquals('function', mirror.type());
  assertFalse(mirror.isPrimitive());
  assertEquals("Function", mirror.className());
  assertEquals(f.name, mirror.name());
  assertTrue(mirror.resolved());
  assertEquals(f.toString(), mirror.source());
  assertTrue(mirror.constructorFunction() instanceof debug.ObjectMirror);
  assertTrue(mirror.protoObject() instanceof debug.Mirror);
  assertTrue(mirror.prototypeObject() instanceof debug.Mirror);

  // Test text representation
  assertEquals(f.toString(), mirror.toText());

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('function', fromJSON.type);
  assertEquals('Function', fromJSON.className);
  assertEquals('function', refs.lookup(fromJSON.constructorFunction.ref).type);
  assertEquals('AsyncFunction',
               refs.lookup(fromJSON.constructorFunction.ref).name);
  assertTrue(fromJSON.resolved);
  assertEquals(f.name, fromJSON.name);
  assertEquals(f.toString(), fromJSON.source);

  // Check the formatted text (regress 1142074).
  assertEquals(f.toString(), fromJSON.text);
}


// Test a number of different functions.
testFunctionMirror(async function(){});
testFunctionMirror(AsyncFunction());
testFunctionMirror(new AsyncFunction());
testFunctionMirror(async() => {});
testFunctionMirror(async function a(){return 1;});
testFunctionMirror(({ async foo() {} }).foo);
testFunctionMirror((async function(){}).bind({}), "Object");

%RunMicrotasks();
