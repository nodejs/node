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
// Test the mirror object for regular error objects

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

function testErrorMirror(e) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(e);
  var serializer = debug.MakeMirrorSerializer();
  var json = serializer.serializeValue(mirror);
  var refs = new MirrorRefCache(serializer.serializeReferencedObjects());

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.ObjectMirror);
  assertTrue(mirror instanceof debug.ErrorMirror);

  // Check the mirror properties.
  assertTrue(mirror.isError());
  assertEquals('error', mirror.type());
  assertFalse(mirror.isPrimitive());
  assertEquals(mirror.message(), e.message, 'source');

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('error', fromJSON.type);
  assertEquals('Error', fromJSON.className);
  if (e.message) {
    var found_message = false;
    for (var i in fromJSON.properties) {
      var p = fromJSON.properties[i];
      print(p.name);
      if (p.name == 'message') {
        assertEquals(e.message, refs.lookup(p.ref).value);
        found_message = true;
      }
    }
    assertTrue(found_message, 'Property message not found');
  }
  
  // Check the formatted text (regress 1231579).
  assertEquals(fromJSON.text, e.toString(), 'toString');
}


// Test Date values.
testErrorMirror(new Error());
testErrorMirror(new Error('This does not work'));
testErrorMirror(new Error(123+456));
testErrorMirror(new EvalError('EvalError'));
testErrorMirror(new RangeError('RangeError'));
testErrorMirror(new ReferenceError('ReferenceError'));
testErrorMirror(new SyntaxError('SyntaxError'));
testErrorMirror(new TypeError('TypeError'));
testErrorMirror(new URIError('URIError'));
