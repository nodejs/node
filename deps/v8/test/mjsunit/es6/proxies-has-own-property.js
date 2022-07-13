// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var handler = {};
var target = {a:1};
var proxy = new Proxy(target, handler);

assertTrue(target.hasOwnProperty('a'));
assertTrue(proxy.hasOwnProperty('a'));
assertFalse(target.hasOwnProperty('b'));
assertFalse(proxy.hasOwnProperty('b'));


handler.has = function() { assertUnreachable() }
handler.getOwnPropertyDescriptor = function () {}

assertTrue(target.hasOwnProperty('a'));
assertFalse(proxy.hasOwnProperty('a'));
assertFalse(target.hasOwnProperty('b'));
assertFalse(proxy.hasOwnProperty('b'));


handler.getOwnPropertyDescriptor = function() { return {configurable: true} }

assertTrue(target.hasOwnProperty('a'));
assertTrue(proxy.hasOwnProperty('a'));
assertFalse(target.hasOwnProperty('b'));
assertTrue(proxy.hasOwnProperty('b'));


handler.getOwnPropertyDescriptor = function() { throw Error(); }

assertTrue(target.hasOwnProperty('a'));
assertThrows(function(){ proxy.hasOwnProperty('a') }, Error);
assertFalse(target.hasOwnProperty('b'));
assertThrows(function(){ proxy.hasOwnProperty('b') }, Error);
