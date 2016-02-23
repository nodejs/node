// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexps --harmony-unicode-regexps

RegExp.prototype.flags = 'setter should be undefined';

assertEquals('', RegExp('').flags);
assertEquals('', /./.flags);
assertEquals('gimuy', RegExp('', 'yugmi').flags);
assertEquals('gimuy', /foo/yumig.flags);

var descriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, 'flags');
assertTrue(descriptor.configurable);
assertFalse(descriptor.enumerable);
assertInstanceof(descriptor.get, Function);
assertEquals(undefined, descriptor.set);

function testGenericFlags(object) {
  return descriptor.get.call(object);
}

assertEquals('', testGenericFlags({}));
assertEquals('i', testGenericFlags({ ignoreCase: true }));
assertEquals('uy', testGenericFlags({ global: 0, sticky: 1, unicode: 1 }));
assertEquals('m', testGenericFlags({ __proto__: { multiline: true } }));
assertThrows(function() { testGenericFlags(); }, TypeError);
assertThrows(function() { testGenericFlags(undefined); }, TypeError);
assertThrows(function() { testGenericFlags(null); }, TypeError);
assertThrows(function() { testGenericFlags(true); }, TypeError);
assertThrows(function() { testGenericFlags(false); }, TypeError);
assertThrows(function() { testGenericFlags(''); }, TypeError);
assertThrows(function() { testGenericFlags(42); }, TypeError);

var counter = 0;
var map = {};
var object = {
  get global() {
    map.g = counter++;
  },
  get ignoreCase() {
    map.i = counter++;
  },
  get multiline() {
    map.m = counter++;
  },
  get unicode() {
    map.u = counter++;
  },
  get sticky() {
    map.y = counter++;
  }
};
testGenericFlags(object);
assertEquals({ g: 0, i: 1, m: 2, u: 3, y: 4 }, map);
