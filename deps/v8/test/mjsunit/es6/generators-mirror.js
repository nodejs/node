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

function TestGeneratorMirror(g, test) {
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

  assertTrue(mirror.receiver().isUndefined());
  assertEquals(generator, mirror.func().value());

  test(mirror);
}

var iter = generator(function () {
  assertEquals('running', debug.MakeMirror(iter).status());
})

// Note that line numbers are 0-based, not 1-based.
function assertSourceLocation(loc, line, column) {
  assertEquals(line, loc.line);
  assertEquals(column, loc.column);
}

TestGeneratorMirror(iter, function (mirror) {
  assertEquals('suspended', mirror.status())
  assertSourceLocation(mirror.sourceLocation(), 7, 19);
});

iter.next();
TestGeneratorMirror(iter, function (mirror) {
  assertEquals('suspended', mirror.status())
  assertSourceLocation(mirror.sourceLocation(), 9, 2);
});

iter.next();
TestGeneratorMirror(iter, function (mirror) {
  assertEquals('suspended', mirror.status())
  assertSourceLocation(mirror.sourceLocation(), 11, 2);
});

iter.next();
TestGeneratorMirror(iter, function (mirror) {
  assertEquals('closed', mirror.status())
  assertEquals(undefined, mirror.sourceLocation());
});
