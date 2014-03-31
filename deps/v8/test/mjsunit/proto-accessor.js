// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --harmony-symbols

// Fake Symbol if undefined, allowing test to run in non-Harmony mode as well.
this.Symbol = typeof Symbol != 'undefined' ? Symbol : String;


var desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
var getProto = desc.get;
var setProto = desc.set;

function TestNoPoisonPill() {
  assertEquals("function", typeof desc.get);
  assertEquals("function", typeof desc.set);
  assertDoesNotThrow("desc.get.call({})");
  assertDoesNotThrow("desc.set.call({}, {})");

  var obj = {};
  var obj2 = {};
  desc.set.call(obj, obj2);
  assertEquals(obj.__proto__, obj2);
  assertEquals(desc.get.call(obj), obj2);
}
TestNoPoisonPill();


function TestRedefineObjectPrototypeProtoGetter() {
  Object.defineProperty(Object.prototype, "__proto__", {
    get: function() {
      return 42;
    }
  });
  assertEquals({}.__proto__, 42);
  assertEquals(desc.get.call({}), Object.prototype);

  var desc2 = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
  assertEquals(desc2.get.call({}), 42);
  assertEquals(desc2.set.call({}), undefined);

  Object.defineProperty(Object.prototype, "__proto__", {
    set: function(x) {}
  });
  var desc3 = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
  assertEquals(desc3.get.call({}), 42);
  assertEquals(desc3.set.call({}), undefined);
}
TestRedefineObjectPrototypeProtoGetter();


function TestRedefineObjectPrototypeProtoSetter() {
  Object.defineProperty(Object.prototype, "__proto__", { set: undefined });
  assertThrows(function() {
    "use strict";
    var o = {};
    var p = {};
    o.__proto__ = p;
  }, TypeError);
}
TestRedefineObjectPrototypeProtoSetter();


function TestGetProtoOfValues() {
  assertEquals(getProto.call(1), Number.prototype);
  assertEquals(getProto.call(true), Boolean.prototype);
  assertEquals(getProto.call(false), Boolean.prototype);
  assertEquals(getProto.call('s'), String.prototype);
  assertEquals(getProto.call(Symbol()), Symbol.prototype);

  assertThrows(function() { getProto.call(null); }, TypeError);
  assertThrows(function() { getProto.call(undefined); }, TypeError);
}
TestGetProtoOfValues();


var values = [1, true, false, 's', Symbol()];


function TestSetProtoOfValues() {
  var proto = {};
  for (var i = 0; i < values.length; i++) {
    assertEquals(setProto.call(values[i], proto), undefined);
  }

  assertThrows(function() { setProto.call(null, proto); }, TypeError);
  assertThrows(function() { setProto.call(undefined, proto); }, TypeError);
}
TestSetProtoOfValues();


function TestSetProtoToValue() {
  var object = {};
  var proto = {};
  setProto.call(object, proto);

  var valuesWithUndefined = values.concat(undefined);

  for (var i = 0; i < valuesWithUndefined.length; i++) {
    assertEquals(setProto.call(object, valuesWithUndefined[i]), undefined);
    assertEquals(getProto.call(object), proto);
  }

  // null is the only valid value that can be used as a [[Prototype]].
  assertEquals(setProto.call(object, null), undefined);
  assertEquals(getProto.call(object), null);
}
TestSetProtoToValue();


function TestDeleteProto() {
  assertTrue(delete Object.prototype.__proto__);
  var o = {};
  var p = {};
  o.__proto__ = p;
  assertEquals(Object.getPrototypeOf(o), Object.prototype);
  var desc4 = Object.getOwnPropertyDescriptor(o, "__proto__");
  assertTrue(desc4.configurable);
  assertTrue(desc4.enumerable);
  assertTrue(desc4.writable);
  assertEquals(desc4.value, p);
}
TestDeleteProto();
