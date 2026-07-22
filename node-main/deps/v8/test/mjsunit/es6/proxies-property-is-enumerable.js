// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var handler = {};
var target = { a: 1 };
var proxy = new Proxy(target, handler);

assertTrue(target.propertyIsEnumerable('a'));
assertTrue(proxy.propertyIsEnumerable('a'));
assertFalse(target.propertyIsEnumerable('b'));
assertFalse(proxy.propertyIsEnumerable('b'));

handler.getOwnPropertyDescriptor = function(target, prop) {
  return { configurable: true, enumerable: true, value: 10 };
}
assertTrue(target.propertyIsEnumerable('a'));
assertTrue(proxy.propertyIsEnumerable('a'));
assertFalse(target.propertyIsEnumerable('b'));
assertTrue(proxy.propertyIsEnumerable('b'));

handler.getOwnPropertyDescriptor = function(target, prop) {
  return { configurable: true, enumerable: false, value: 10 };
}
assertTrue(target.propertyIsEnumerable('a'));
assertFalse(proxy.propertyIsEnumerable('a'));
assertFalse(target.propertyIsEnumerable('b'));
assertFalse(proxy.propertyIsEnumerable('b'));
