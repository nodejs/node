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

function props(x) {
  var array = [];
  for (var p in x) array.push(p);
  return array;
}

(function forInBasic() {
  assertEquals(0, props({}).length, "olen0");
  assertEquals(1, props({x:1}).length, "olen1");
  assertEquals(2, props({x:1, y:2}).length, "olen2");

  assertArrayEquals(["x"], props({x:1}), "x");
  assertArrayEquals(["x", "y"], props({x:1, y:2}), "xy");
  assertArrayEquals(["x", "y", "zoom"], props({x:1, y:2, zoom:3}), "xyzoom");

  assertEquals(0, props([]).length, "alen0");
  assertEquals(1, props([1]).length, "alen1");
  assertEquals(2, props([1,2]).length, "alen2");

  assertArrayEquals(["0"], props([1]), "0");
  assertArrayEquals(["0", "1"], props([1,2]), "01");
  assertArrayEquals(["0", "1", "2"], props([1,2,3]), "012");
})();

(function forInPrototype() {
  // Fast properties + fast elements
  var obj = {a:true, 3:true, 4:true};
  obj.__proto__ = {c:true, b:true, 2:true, 1:true, 5:true};
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("34a125cb".split(""), props(obj));
  }
  // Fast properties + dictionary elements
  delete obj.__proto__[2];
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("34a15cb".split(""), props(obj));
  }
  // Slow properties + dictionary elements
  delete obj.__proto__.c;
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("34a15b".split(""), props(obj));
  }
  // Slow properties on the receiver as well
  delete obj.a;
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("3415b".split(""), props(obj));
  }
  delete obj[3];
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("415b".split(""), props(obj));
  }
})();

(function forInShadowing() {
  var obj = {a:true, 3:true, 4:true};
  obj.__proto__ = {
    c:true, b:true, x:true,
    2:true, 1:true, 5:true, 9:true};
  Object.defineProperty(obj, 'x', {value:true, enumerable:false, configurable:true});
  Object.defineProperty(obj, '9', {value:true, enumerable:false, configurable:true});
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("34a125cb".split(""), props(obj));
  }
  // Fast properties + dictionary elements
  delete obj.__proto__[2];
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("34a15cb".split(""), props(obj));
  }
  // Slow properties + dictionary elements
  delete obj.__proto__.c;
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("34a15b".split(""), props(obj));
  }
  // Remove the shadowing properties
  delete obj.x;
  delete obj[9];
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("34a159bx".split(""), props(obj));
  }
  // Slow properties on the receiver as well
  delete obj.a;
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("34159bx".split(""), props(obj));
  }
  delete obj[3];
  for (var i = 0; i < 3; i++) {
    assertArrayEquals("4159bx".split(""), props(obj));
  }
})();

(function forInShadowingSlowReceiver() {
  // crbug 688307
  // Make sure we track all non-enumerable keys on a slow-mode receiver.
  let receiver = {a:1};
  delete receiver.a;
  let proto = Object.create(null);
  let enumProperties = [];
  for (let i = 0; i < 10; i++) {
    let key = "property_"+i;
    enumProperties.push(key);
    receiver[key] = i;
    proto[key] = i;
  }
  for (let i = 0; i < 1000; i++) {
    let nonEnumKey = "nonEnumerableProperty_"+ i;
    Object.defineProperty(receiver, nonEnumKey, {});
    // Add both keys as enumerable to the prototype.
    proto[nonEnumKey] = i;
  }
  receiver.__proto__ = proto;
  // Only the enumerable properties from the receiver should be visible.
  for (let key in receiver) {
    assertEquals(key, enumProperties.shift());
  }
})();

(function forInCharCodes() {
  var o = {};
  var a = [];
  for (var i = 0x0020; i < 0x01ff; i+=2) {
    var s = 'char:' + String.fromCharCode(i);
    a.push(s);
    o[s] = i;
  }
  assertArrayEquals(a, props(o), "charcodes");
})();

(function forInArray() {
  var a = [];
  assertEquals(0, props(a).length, "proplen0");
  a[Math.pow(2,30)-1] = 0;
  assertEquals(1, props(a).length, "proplen1");
  a[Math.pow(2,31)-1] = 0;
  assertEquals(2, props(a).length, "proplen2");
  a[1] = 0;
  assertEquals(3, props(a).length, "proplen3");
})();

(function forInInitialize() {
  for (var hest = 'hest' in {}) { }
  assertEquals('hest', hest, "empty-no-override");

  // Lexical variables are disallowed
  assertThrows("for (const x = 0 in {});", SyntaxError);
  assertThrows("for (let x = 0 in {});", SyntaxError);

  // In strict mode, var is disallowed
  assertThrows("'use strict'; for (var x = 0 in {});", SyntaxError);
})();

(function forInObjects() {
  var result = '';
  for (var p in {a : [0], b : 1}) { result += p; }
  assertEquals('ab', result, "ab");

  var result = '';
  for (var p in {a : {v:1}, b : 1}) { result += p; }
  assertEquals('ab', result, "ab-nodeep");

  var result = '';
  for (var p in { get a() {}, b : 1}) { result += p; }
  assertEquals('ab', result, "abget");

  var result = '';
  for (var p in { get a() {}, set a(x) {}, b : 1}) { result += p; }
  assertEquals('ab', result, "abgetset");
})();


// Test that for-in in the global scope works with a keyed property as "each".
// Test outside a loop and in a loop for multiple iterations.
a = [1,2,3,4];
x = {foo:5, bar:6, zip:7, glep:9, 10:11};
delete x.bar;
y = {}

for (a[2] in x) {
  y[a[2]] = x[a[2]];
}

assertEquals(5, y.foo, "y.foo");
assertEquals("undefined", typeof y.bar, "y.bar");
assertEquals(7, y.zip, "y.zip");
assertEquals(9, y.glep, "y.glep");
assertEquals(11, y[10], "y[10]");
assertEquals("undefined", typeof y[2], "y[2]");
assertEquals("undefined", typeof y[0], "y[0]");

for (i=0 ; i < 3; ++i) {
  y = {}

  for (a[2] in x) {
    y[a[2]] = x[a[2]];
  }

  assertEquals(5, y.foo, "y.foo");
  assertEquals("undefined", typeof y.bar, "y.bar");
  assertEquals(7, y.zip, "y.zip");
  assertEquals(9, y.glep, "y.glep");
  assertEquals(11, y[10], "y[10]");
  assertEquals("undefined", typeof y[2], "y[2]");
  assertEquals("undefined", typeof y[0], "y[0]");
}

(function testLargeElementKeys() {
  // Key out of SMI range but well within safe double representaion.
  var large_key = 2147483650;
  var o = [];
  // Trigger dictionary elements with HeapNumber keys.
  o[large_key] = 0;
  o[large_key+1] = 1;
  o[large_key+2] = 2;
  o[large_key+3] = 3;
  var keys = [];
  for (var k in o) {
    keys.push(k);
  }
  assertEquals(["2147483650", "2147483651", "2147483652", "2147483653"], keys);
})();

(function testLargeElementKeysWithProto() {
  var large_key = 2147483650;
  var o = {__proto__: {}};
  o[large_key] = 1;
  o.__proto__[large_key] = 1;
  var keys = [];
  for (var k in o) {
    keys.push(k);
  }
  assertEquals(["2147483650"], keys);
})();

(function testNonEnumerableArgumentsIndex() {
  Object.defineProperty(arguments, 0, {enumerable:false});
  for (var k in arguments) {
    assertUnreachable();
  }
})();

(function testNonEnumerableSloppyArgumentsIndex(a) {
  Object.defineProperty(arguments, 0, {enumerable:false});
  for (var k in arguments) {
    assertUnreachable();
  }
})(true);
