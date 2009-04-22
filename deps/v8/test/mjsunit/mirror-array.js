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
// Test the mirror object for objects

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

function testArrayMirror(a, names) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(a);
  var serializer = debug.MakeMirrorSerializer();
  var json = serializer.serializeValue(mirror);
  var refs = new MirrorRefCache(serializer.serializeReferencedObjects());

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror, 'Unexpected mirror hierachy');
  assertTrue(mirror instanceof debug.ValueMirror, 'Unexpected mirror hierachy');
  assertTrue(mirror instanceof debug.ObjectMirror, 'Unexpected mirror hierachy');
  assertTrue(mirror instanceof debug.ArrayMirror, 'Unexpected mirror hierachy');

  // Check the mirror properties.
  assertTrue(mirror.isArray(), 'Unexpected mirror');
  assertEquals('object', mirror.type(), 'Unexpected mirror type');
  assertFalse(mirror.isPrimitive(), 'Unexpected primitive mirror');
  assertEquals('Array', mirror.className(), 'Unexpected mirror class name');
  assertTrue(mirror.constructorFunction() instanceof debug.ObjectMirror, 'Unexpected mirror hierachy');
  assertEquals('Array', mirror.constructorFunction().name(), 'Unexpected constructor function name');
  assertTrue(mirror.protoObject() instanceof debug.Mirror, 'Unexpected mirror hierachy');
  assertTrue(mirror.prototypeObject() instanceof debug.Mirror, 'Unexpected mirror hierachy');
  assertEquals(mirror.length(), a.length, "Length mismatch");
  
  var indexedProperties = mirror.indexedPropertiesFromRange();
  assertEquals(indexedProperties.length, a.length);
  for (var i = 0; i < indexedProperties.length; i++) {
    assertTrue(indexedProperties[i] instanceof debug.Mirror, 'Unexpected mirror hierachy');
    assertTrue(indexedProperties[i] instanceof debug.PropertyMirror, 'Unexpected mirror hierachy');
  }

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('object', fromJSON.type, 'Unexpected mirror type in JSON');
  assertEquals('Array', fromJSON.className, 'Unexpected mirror class name in JSON');
  assertEquals(mirror.constructorFunction().handle(), fromJSON.constructorFunction.ref, 'Unexpected constructor function handle in JSON');
  assertEquals('function', refs.lookup(fromJSON.constructorFunction.ref).type, 'Unexpected constructor function type in JSON');
  assertEquals('Array', refs.lookup(fromJSON.constructorFunction.ref).name, 'Unexpected constructor function name in JSON');
  assertEquals(void 0, fromJSON.namedInterceptor, 'No named interceptor expected in JSON');
  assertEquals(void 0, fromJSON.indexedInterceptor, 'No indexed interceptor expected in JSON');

  // Check that the serialization contains all indexed properties and the length property.
  var length_found = false;
  for (var i = 0; i < fromJSON.properties.length; i++) {
    if (fromJSON.properties[i].name == 'length') {
      length_found = true;
      assertEquals('number', refs.lookup(fromJSON.properties[i].ref).type, "Unexpected type of the length property");
      assertEquals(a.length, refs.lookup(fromJSON.properties[i].ref).value, "Length mismatch in parsed JSON");
    } else {
      var index = parseInt(fromJSON.properties[i].name);
        print(index);
      if (!isNaN(index)) {
        print(index);
        // This test assumes that the order of the indexeed properties is in the
        // same order in the serialization as returned from
        // indexedPropertiesFromRange()
        assertEquals(indexedProperties[index].name(), index);
        assertEquals(indexedProperties[index].value().type(), refs.lookup(fromJSON.properties[i].ref).type, 'Unexpected serialized type');
      }
    }
  }
  assertTrue(length_found, 'Property length not found');

  // Check that the serialization contains all names properties.
  if (names) {
    for (var i = 0; i < names.length; i++) {
      var found = false;
      for (var j = 0; j < fromJSON.properties.length; j++) {
        if (names[i] == fromJSON.properties[j].name) {
          found = true; 
        }
      }
      assertTrue(found, names[i])
    }
  }
}


// Test a number of different arrays.
testArrayMirror([]);
testArrayMirror([1]);
testArrayMirror([1,2]);
testArrayMirror(["a", function(){}, [1,2], 2, /[ab]/]);

a=[1];
a[100]=7;
testArrayMirror(a);

a=[1,2,3];
a.x=2.2;
a.y=function(){return null;}
testArrayMirror(a, ['x','y']);

var a = []; a.push(a);
testArrayMirror(a);
