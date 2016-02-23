// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --expose-debug-as debug
// Test the mirror object for functions.

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
  assertEquals('Function', refs.lookup(fromJSON.constructorFunction.ref).name);
  assertTrue(fromJSON.resolved);
  assertEquals(f.name, fromJSON.name);
  assertEquals(f.toString(), fromJSON.source);

  // Check the formatted text (regress 1142074).
  assertEquals(f.toString(), fromJSON.text);
}


// Test a number of different functions.
testFunctionMirror(function(){});
testFunctionMirror(function a(){return 1;});
testFunctionMirror(Math.sin);
