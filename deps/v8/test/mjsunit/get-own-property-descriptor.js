// Copyright 2010 the V8 project authors. All rights reserved.
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

// This file only tests very simple descriptors that always have
// configurable, enumerable, and writable set to true.
// A range of more elaborate tests are performed in 
// object-define-property.js

function get() { return x; }
function set(x) { this.x = x; }

var obj = {x: 1};
obj.__defineGetter__("accessor", get);
obj.__defineSetter__("accessor", set);
var a = new Array();
a[1] = 42;
obj[1] = 42;

var descIsData = Object.getOwnPropertyDescriptor(obj, 'x');
assertTrue(descIsData.enumerable);
assertTrue(descIsData.writable);
assertTrue(descIsData.configurable);

var descIsAccessor = Object.getOwnPropertyDescriptor(obj, 'accessor');
assertTrue(descIsAccessor.enumerable);
assertTrue(descIsAccessor.configurable);
assertTrue(descIsAccessor.get == get);
assertTrue(descIsAccessor.set == set);

var descIsNotData = Object.getOwnPropertyDescriptor(obj, 'not-x');
assertTrue(descIsNotData == undefined);

var descIsNotAccessor = Object.getOwnPropertyDescriptor(obj, 'not-accessor');
assertTrue(descIsNotAccessor == undefined);

var descArray = Object.getOwnPropertyDescriptor(a, '1');
assertTrue(descArray.enumerable);
assertTrue(descArray.configurable);
assertTrue(descArray.writable);
assertEquals(descArray.value, 42);

var descObjectElement = Object.getOwnPropertyDescriptor(obj, '1');
assertTrue(descObjectElement.enumerable);
assertTrue(descObjectElement.configurable);
assertTrue(descObjectElement.writable);
assertEquals(descObjectElement.value, 42);

// String objects.
var a = new String('foobar');
for (var i = 0; i < a.length; i++) {
  var descStringObject = Object.getOwnPropertyDescriptor(a, i);
  assertFalse(descStringObject.enumerable);
  assertFalse(descStringObject.configurable);
  assertFalse(descStringObject.writable);
  assertEquals(descStringObject.value, a.substring(i, i+1));
}

// Support for additional attributes on string objects.
a.x = 42;
a[10] = 'foo';
var descStringProperty = Object.getOwnPropertyDescriptor(a, 'x');
assertTrue(descStringProperty.enumerable);
assertTrue(descStringProperty.configurable);
assertTrue(descStringProperty.writable);
assertEquals(descStringProperty.value, 42);

var descStringElement = Object.getOwnPropertyDescriptor(a, '10');
assertTrue(descStringElement.enumerable);
assertTrue(descStringElement.configurable);
assertTrue(descStringElement.writable);
assertEquals(descStringElement.value, 'foo');

// Test that elements in the prototype chain is not returned.
var proto = {};
proto[10] = 42;

var objWithProto = new Array();
objWithProto.prototype = proto;
objWithProto[0] = 'bar';
var descWithProto = Object.getOwnPropertyDescriptor(objWithProto, '10');
assertEquals(undefined, descWithProto);

// Test elements on global proxy object.
var global = (function() { return this; })();

global[42] = 42;

function el_getter() { return 239; };
function el_setter() {};
Object.defineProperty(global, '239', {get: el_getter, set: el_setter});

var descRegularElement = Object.getOwnPropertyDescriptor(global, '42');
assertEquals(42, descRegularElement.value);

var descAccessorElement = Object.getOwnPropertyDescriptor(global, '239');
assertEquals(el_getter, descAccessorElement.get);
assertEquals(el_setter, descAccessorElement.set);
