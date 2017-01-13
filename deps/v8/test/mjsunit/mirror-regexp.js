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
// Test the mirror object for regular expression values

var dont_enum = debug.PropertyAttribute.DontEnum;
var dont_delete = debug.PropertyAttribute.DontDelete;
var expected_prototype_attributes = {
  'source': dont_enum,
  'global': dont_enum,
  'ignoreCase': dont_enum,
  'multiline': dont_enum,
  'unicode' : dont_enum,
};

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

function testRegExpMirror(r) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(r);
  var serializer = debug.MakeMirrorSerializer();
  var json = JSON.stringify(serializer.serializeValue(mirror));
  var refs = new MirrorRefCache(
      JSON.stringify(serializer.serializeReferencedObjects()));

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.ObjectMirror);
  assertTrue(mirror instanceof debug.RegExpMirror);

  // Check the mirror properties.
  assertTrue(mirror.isRegExp());
  assertEquals('regexp', mirror.type());
  assertFalse(mirror.isPrimitive());
  assertEquals(dont_enum | dont_delete,
               mirror.property('lastIndex').attributes());
  var proto_mirror = mirror.protoObject();
  for (var p in expected_prototype_attributes) {
    assertEquals(expected_prototype_attributes[p],
                 proto_mirror.property(p).attributes(),
                 p + ' attributes');
  }

  // Test text representation
  assertEquals('/' + r.source + '/', mirror.toText());

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('regexp', fromJSON.type);
  assertEquals('RegExp', fromJSON.className);
  assertEquals('lastIndex', fromJSON.properties[0].name);
  assertEquals(dont_enum | dont_delete, fromJSON.properties[0].attributes);
  assertEquals(mirror.property('lastIndex').propertyType(),
               fromJSON.properties[0].propertyType);
  assertEquals(mirror.property('lastIndex').value().value(),
               refs.lookup(fromJSON.properties[0].ref).value);
}


// Test Date values.
testRegExpMirror(/x/);
testRegExpMirror(/[abc]/);
testRegExpMirror(/[\r\n]/g);
testRegExpMirror(/a*b/gmi);
testRegExpMirror(/(\u{0066}|\u{0062})oo/u);
