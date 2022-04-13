// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var ab1 = new ArrayBuffer(8);
ab1.__defineGetter__("byteLength", function() { return 1000000; });
var ab2 = ab1.slice(800000, 900000);
var array = new Uint8Array(ab2);
for (var i = 0; i < array.length; i++) {
  assertEquals(0, array[i]);
}
assertEquals(0, array.length);


var ab3 = new ArrayBuffer(8);
ab3.__defineGetter__("byteLength", function() { return 0xFFFFFFFC; });
var aaa = new DataView(ab3);

for (var i = 10; i < aaa.length; i++) {
  aaa.setInt8(i, 0xcc);
}
assertEquals(8, aaa.byteLength);


var a = new Int8Array(4);
a.__defineGetter__("length", function() { return 0xFFFF; });
var b = new Int8Array(a);
for (var i = 0; i < b.length; i++) {
  assertEquals(0, b[i]);
}


var ab4 = new ArrayBuffer(8);
ab4.__defineGetter__("byteLength", function() { return 0xFFFFFFFC; });
var aaaa = new Uint32Array(ab4);

for (var i = 10; i < aaaa.length; i++) {
  aaaa[i] = 0xcccccccc;
}
assertEquals(2, aaaa.length);
