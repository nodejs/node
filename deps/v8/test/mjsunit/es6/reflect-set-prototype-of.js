// Copyright 2014-2015 the V8 project authors. All rights reserved.
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

// This is adapted from mjsunit/harmony/set-prototype-of.js.



function getObjects() {
  function func() {}
  return [
    func,
    new func(),
    {x: 5},
    /regexp/,
    ['array'],
    // new Error(),
    new Date(),
    new Number(1),
    new Boolean(true),
    new String('str'),
    Object(Symbol())
  ];
}


var coercibleValues = [
  1,
  true,
  'string',
  Symbol()
];


var nonCoercibleValues = [
  undefined,
  null
];


var valuesWithoutNull = coercibleValues.concat(undefined);


function TestSetPrototypeOfCoercibleValues() {
  for (var i = 0; i < coercibleValues.length; i++) {
    var value = coercibleValues[i];
    var proto = Object.getPrototypeOf(value);
    assertThrows(function() { Reflect.setPrototypeOf(value, {}) }, TypeError);
    assertSame(proto, Object.getPrototypeOf(value));
  }
}
TestSetPrototypeOfCoercibleValues();


function TestSetPrototypeOfNonCoercibleValues() {
  for (var i = 0; i < nonCoercibleValues.length; i++) {
    var value = nonCoercibleValues[i];
    assertThrows(function() {
      Reflect.setPrototypeOf(value, {});
    }, TypeError);
  }
}
TestSetPrototypeOfNonCoercibleValues();


function TestSetPrototypeToNonObject(proto) {
  var objects = getObjects();
  for (var i = 0; i < objects.length; i++) {
    var object = objects[i];
    for (var j = 0; j < valuesWithoutNull.length; j++) {
      var proto = valuesWithoutNull[j];
      assertThrows(function() {
        Reflect.setPrototypeOf(object, proto);
      }, TypeError);
    }
  }
}
TestSetPrototypeToNonObject();


function TestSetPrototypeOf(object, proto) {
  assertTrue(Reflect.setPrototypeOf(object, proto));
  assertEquals(Object.getPrototypeOf(object), proto);
}


function TestSetPrototypeOfForObjects() {
  var objects1 = getObjects();
  var objects2 = getObjects();
  for (var i = 0; i < objects1.length; i++) {
    for (var j = 0; j < objects2.length; j++) {
      TestSetPrototypeOf(objects1[i], objects2[j]);
    }
  }
}
TestSetPrototypeOfForObjects();


function TestSetPrototypeToNull() {
  var objects = getObjects();
  for (var i = 0; i < objects.length; i++) {
    TestSetPrototypeOf(objects[i], null);
  }
}
TestSetPrototypeToNull();


function TestSetPrototypeOfNonExtensibleObject() {
  var objects = getObjects();
  var proto = {};
  for (var i = 0; i < objects.length; i++) {
    var object = objects[i];
    Object.preventExtensions(object);
    // Setting the current prototype must succeed.
    assertTrue(Reflect.setPrototypeOf(object, Object.getPrototypeOf(object)));
    // Setting any other must fail.
    assertFalse(Reflect.setPrototypeOf(object, proto));
  }
}
TestSetPrototypeOfNonExtensibleObject();


function TestSetPrototypeCyclic() {
  var objects = [
    Object.prototype, {},
    Array.prototype, [],
    Error.prototype, new TypeError,
    // etc ...
  ];
  for (var i = 0; i < objects.length; i += 2) {
    var object = objects[i];
    var value = objects[i + 1];
    assertFalse(Reflect.setPrototypeOf(object, value));
  }
}
TestSetPrototypeCyclic();


function TestLookup() {
  var object = {};
  assertFalse('x' in object);
  assertFalse('y' in object);

  var oldProto = {
    x: 'old x',
    y: 'old y'
  };
  assertTrue(Reflect.setPrototypeOf(object, oldProto));
  assertEquals(object.x, 'old x');
  assertEquals(object.y, 'old y');

  var newProto = {
    x: 'new x'
  };
  assertTrue(Reflect.setPrototypeOf(object, newProto));
  assertEquals(object.x, 'new x');
  assertFalse('y' in object);
}
TestLookup();
