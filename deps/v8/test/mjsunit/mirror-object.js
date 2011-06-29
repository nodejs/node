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
};

function testObjectMirror(obj, cls_name, ctor_name, hasSpecialProperties) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(obj);
  var serializer = debug.MakeMirrorSerializer();
  var json = JSON.stringify(serializer.serializeValue(mirror));
  var refs = new MirrorRefCache(
      JSON.stringify(serializer.serializeReferencedObjects()));

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror, 'Unexpected mirror hierachy');
  assertTrue(mirror instanceof debug.ValueMirror, 'Unexpected mirror hierachy');
  assertTrue(mirror instanceof debug.ObjectMirror, 'Unexpected mirror hierachy');

  // Check the mirror properties.
  assertTrue(mirror.isObject(), 'Unexpected mirror');
  assertEquals('object', mirror.type(), 'Unexpected mirror type');
  assertFalse(mirror.isPrimitive(), 'Unexpected primitive mirror');
  assertEquals(cls_name, mirror.className(), 'Unexpected mirror class name');
  assertTrue(mirror.constructorFunction() instanceof debug.ObjectMirror, 'Unexpected mirror hierachy');
  assertEquals(ctor_name, mirror.constructorFunction().name(), 'Unexpected constructor function name');
  assertTrue(mirror.protoObject() instanceof debug.Mirror, 'Unexpected mirror hierachy');
  assertTrue(mirror.prototypeObject() instanceof debug.Mirror, 'Unexpected mirror hierachy');
  assertFalse(mirror.hasNamedInterceptor(), 'No named interceptor expected');
  assertFalse(mirror.hasIndexedInterceptor(), 'No indexed interceptor expected');

  var names = mirror.propertyNames();
  var properties = mirror.properties();
  assertEquals(names.length, properties.length);
  for (var i = 0; i < properties.length; i++) {
    assertTrue(properties[i] instanceof debug.Mirror, 'Unexpected mirror hierachy');
    assertTrue(properties[i] instanceof debug.PropertyMirror, 'Unexpected mirror hierachy');
    assertEquals('property', properties[i].type(), 'Unexpected mirror type');
    assertEquals(names[i], properties[i].name(), 'Unexpected property name');
  }

  for (var p in obj) {
    var property_mirror = mirror.property(p);
    assertTrue(property_mirror instanceof debug.PropertyMirror);
    assertEquals(p, property_mirror.name());
    // If the object has some special properties don't test for these.
    if (!hasSpecialProperties) {
      assertEquals(0, property_mirror.attributes(), property_mirror.name());
      assertFalse(property_mirror.isReadOnly());
      assertTrue(property_mirror.isEnum());
      assertTrue(property_mirror.canDelete());
    }
  }

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('object', fromJSON.type, 'Unexpected mirror type in JSON');
  assertEquals(cls_name, fromJSON.className, 'Unexpected mirror class name in JSON');
  assertEquals(mirror.constructorFunction().handle(), fromJSON.constructorFunction.ref, 'Unexpected constructor function handle in JSON');
  assertEquals('function', refs.lookup(fromJSON.constructorFunction.ref).type, 'Unexpected constructor function type in JSON');
  assertEquals(ctor_name, refs.lookup(fromJSON.constructorFunction.ref).name, 'Unexpected constructor function name in JSON');
  assertEquals(mirror.protoObject().handle(), fromJSON.protoObject.ref, 'Unexpected proto object handle in JSON');
  assertEquals(mirror.protoObject().type(), refs.lookup(fromJSON.protoObject.ref).type, 'Unexpected proto object type in JSON');
  assertEquals(mirror.prototypeObject().handle(), fromJSON.prototypeObject.ref, 'Unexpected prototype object handle in JSON');
  assertEquals(mirror.prototypeObject().type(), refs.lookup(fromJSON.prototypeObject.ref).type, 'Unexpected prototype object type in JSON');
  assertEquals(void 0, fromJSON.namedInterceptor, 'No named interceptor expected in JSON');
  assertEquals(void 0, fromJSON.indexedInterceptor, 'No indexed interceptor expected in JSON');

  // Check that the serialization contains all properties.
  assertEquals(names.length, fromJSON.properties.length, 'Some properties missing in JSON');
  for (var i = 0; i < fromJSON.properties.length; i++) {
    var name = fromJSON.properties[i].name;
    if (typeof name == 'undefined') name = fromJSON.properties[i].index;
    var found = false;
    for (var j = 0; j < names.length; j++) {
      if (names[j] == name) {
        // Check that serialized handle is correct.
        assertEquals(properties[i].value().handle(), fromJSON.properties[i].ref, 'Unexpected serialized handle');

        // Check that serialized name is correct.
        assertEquals(properties[i].name(), fromJSON.properties[i].name, 'Unexpected serialized name');

        // If property type is normal property type is not serialized.
        if (properties[i].propertyType() != debug.PropertyType.Normal) {
          assertEquals(properties[i].propertyType(), fromJSON.properties[i].propertyType, 'Unexpected serialized property type');
        } else {
          assertTrue(typeof(fromJSON.properties[i].propertyType) === 'undefined', 'Unexpected serialized property type');
        }

        // If there are no attributes attributes are not serialized.
        if (properties[i].attributes() != debug.PropertyAttribute.None) {
          assertEquals(properties[i].attributes(), fromJSON.properties[i].attributes, 'Unexpected serialized attributes');
        } else {
          assertTrue(typeof(fromJSON.properties[i].attributes) === 'undefined', 'Unexpected serialized attributes');
        }

        // Lookup the serialized object from the handle reference.
        var o = refs.lookup(fromJSON.properties[i].ref);
        assertTrue(o != void 0, 'Referenced object is not serialized');

        assertEquals(properties[i].value().type(), o.type, 'Unexpected serialized property type for ' + name);
        if (properties[i].value().isPrimitive()) {
          if (properties[i].value().type() == "null" ||
              properties[i].value().type() == "undefined") {
            // Null and undefined has no value property.
            assertFalse("value" in o, 'Unexpected value property for ' + name);
          } else if (properties[i].value().type() == "number" &&
                     !isFinite(properties[i].value().value())) {
            assertEquals(String(properties[i].value().value()), o.value,
                         'Unexpected serialized property value for ' + name);
          } else {
            assertEquals(properties[i].value().value(), o.value, 'Unexpected serialized property value for ' + name);
          }
        } else if (properties[i].value().isFunction()) {
          assertEquals(properties[i].value().source(), o.source, 'Unexpected serialized property value for ' + name);
        }
        found = true;
      }
    }
    assertTrue(found, '"' + name + '" not found (' + json + ')');
  }
}


function Point(x,y) {
  this.x_ = x;
  this.y_ = y;
}

// Test a number of different objects.
testObjectMirror({}, 'Object', 'Object');
testObjectMirror({'a':1,'b':2}, 'Object', 'Object');
testObjectMirror({'1':void 0,'2':null,'f':function pow(x,y){return Math.pow(x,y);}}, 'Object', 'Object');
testObjectMirror(new Point(-1.2,2.003), 'Object', 'Point');
testObjectMirror(this, 'global', '', true);  // Global object has special properties
testObjectMirror(this.__proto__, 'Object', '');
testObjectMirror([], 'Array', 'Array');
testObjectMirror([1,2], 'Array', 'Array');

// Test circular references.
o = {};
o.o = o;
testObjectMirror(o, 'Object', 'Object');

// Test that non enumerable properties are part of the mirror
global_mirror = debug.MakeMirror(this);
assertEquals('property', global_mirror.property("Math").type());
assertFalse(global_mirror.property("Math").isEnum(), "Math is enumerable" + global_mirror.property("Math").attributes());

math_mirror = global_mirror.property("Math").value();
assertEquals('property', math_mirror.property("E").type());
assertFalse(math_mirror.property("E").isEnum(), "Math.E is enumerable");
assertTrue(math_mirror.property("E").isReadOnly());
assertFalse(math_mirror.property("E").canDelete());

// Test objects with JavaScript accessors.
o = {}
o.__defineGetter__('a', function(){return 'a';});
o.__defineSetter__('b', function(){});
o.__defineGetter__('c', function(){throw 'c';});
o.__defineSetter__('c', function(){throw 'c';});
testObjectMirror(o, 'Object', 'Object');
mirror = debug.MakeMirror(o);
// a has getter but no setter.
assertTrue(mirror.property('a').hasGetter());
assertFalse(mirror.property('a').hasSetter());
assertEquals(debug.PropertyType.Callbacks, mirror.property('a').propertyType());
assertEquals('function', mirror.property('a').getter().type());
assertEquals('undefined', mirror.property('a').setter().type());
assertEquals('function (){return \'a\';}', mirror.property('a').getter().source());
// b has setter but no getter.
assertFalse(mirror.property('b').hasGetter());
assertTrue(mirror.property('b').hasSetter());
assertEquals(debug.PropertyType.Callbacks, mirror.property('b').propertyType());
assertEquals('undefined', mirror.property('b').getter().type());
assertEquals('function', mirror.property('b').setter().type());
assertEquals('function (){}', mirror.property('b').setter().source());
assertFalse(mirror.property('b').isException());
// c has both getter and setter. The getter throws an exception.
assertTrue(mirror.property('c').hasGetter());
assertTrue(mirror.property('c').hasSetter());
assertEquals(debug.PropertyType.Callbacks, mirror.property('c').propertyType());
assertEquals('function', mirror.property('c').getter().type());
assertEquals('function', mirror.property('c').setter().type());
assertEquals('function (){throw \'c\';}', mirror.property('c').getter().source());
assertEquals('function (){throw \'c\';}', mirror.property('c').setter().source());

// Test objects with native accessors.
mirror = debug.MakeMirror(new String('abc'));
assertTrue(mirror instanceof debug.ObjectMirror);
assertFalse(mirror.property('length').hasGetter());
assertFalse(mirror.property('length').hasSetter());
assertTrue(mirror.property('length').isNative());
assertEquals('a', mirror.property(0).value().value());
assertEquals('b', mirror.property(1).value().value());
assertEquals('c', mirror.property(2).value().value());
