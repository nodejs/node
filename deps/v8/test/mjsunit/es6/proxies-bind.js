// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests the interaction of Function.prototype.bind with proxies.


// (Helper)

var log = [];
var logger = {};
var handler = new Proxy({}, logger);

logger.get = function(t, trap, r) {
  return function() {
    log.push([trap, ...arguments]);
    return Reflect[trap](...arguments);
  }
};


// Simple case

var target = function(a, b, c) { "use strict"; return this };
var proxy = new Proxy(target, handler);
var this_value = Symbol();

log.length = 0;
result = Function.prototype.bind.call(proxy, this_value, "foo");
assertEquals(2, result.length);
assertEquals(target.__proto__, result.__proto__);
assertEquals(this_value, result());
assertEquals(5, log.length);
for (var i in log) assertSame(target, log[i][1]);
assertEquals(["getPrototypeOf", target], log[0]);
assertEquals(["getOwnPropertyDescriptor", target, "length"], log[1]);
assertEquals(["get", target, "length", proxy], log[2]);
assertEquals(["get", target, "name", proxy], log[3]);
assertEquals(["apply", target, this_value, ["foo"]], log[4]);
assertEquals(new target(), new result());


// Custom prototype

log.length = 0;
target.__proto__ = {radio: "gaga"};
result = Function.prototype.bind.call(proxy, this_value, "foo");
assertEquals(2, result.length);
assertSame(target.__proto__, result.__proto__);
assertEquals(this_value, result());
assertEquals(5, log.length);
for (var i in log) assertSame(target, log[i][1]);
assertEquals(["getPrototypeOf", target], log[0]);
assertEquals(["getOwnPropertyDescriptor", target, "length"], log[1]);
assertEquals(["get", target, "length", proxy], log[2]);
assertEquals(["get", target, "name", proxy], log[3]);
assertEquals(["apply", target, this_value, ["foo"]], log[4]);


// Custom length

handler = {
  get() {return 42},
  getOwnPropertyDescriptor() {return {configurable: true}}
};
proxy = new Proxy(target, handler);

result = Function.prototype.bind.call(proxy, this_value, "foo");
assertEquals(41, result.length);
assertEquals(this_value, result());


// Long length

handler = {
  get() {return Math.pow(2, 100)},
  getOwnPropertyDescriptor() {return {configurable: true}}
};
proxy = new Proxy(target, handler);

result = Function.prototype.bind.call(proxy, this_value, "foo");
assertEquals(Math.pow(2, 100) - 1, result.length);
assertEquals(this_value, result());


// Very long length

handler = {
  get() {return 1/0},
  getOwnPropertyDescriptor() {return {configurable: true}}
};
proxy = new Proxy(target, handler);

result = Function.prototype.bind.call(proxy, this_value, "foo");
assertEquals(1/0, result.length);
assertEquals(this_value, result());


// Non-integer length

handler = {
  get() {return 4.2},
  getOwnPropertyDescriptor() {return {configurable: true}}
};
proxy = new Proxy(target, handler);

result = Function.prototype.bind.call(proxy, this_value, "foo");
assertEquals(3, result.length);
assertEquals(this_value, result());


// Undefined length

handler = {
  get() {},
  getOwnPropertyDescriptor() {return {configurable: true}}
};
proxy = new Proxy(target, handler);

result = Function.prototype.bind.call(proxy, this_value, "foo");
assertEquals(0, result.length);
assertEquals(this_value, result());


// Non-callable

assertThrows(() => Function.prototype.bind.call(new Proxy({}, {})), TypeError);
assertThrows(() => Function.prototype.bind.call(new Proxy([], {})), TypeError);


// Non-constructable

result = Function.prototype.bind.call(() => 42, this_value, "foo");
assertEquals(42, result());
assertThrows(() => new result());
