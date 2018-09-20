// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testObjectKeys() {
  var target = {
    target: 1
  };
  target.__proto__ = {
    target_proto: 2
  };

  var handler = {
    ownKeys: function(target) {
      return ["foo", "bar", Symbol("baz"), "non-enum", "not-found"];
    },
    getOwnPropertyDescriptor: function(target, name) {
      if (name == "non-enum") return {configurable: true};
      if (name == "not-found") return undefined;
      return {enumerable: true, configurable: true};
    }
  }

  var proxy = new Proxy(target, handler);

  // Object.keys() ignores symbols and non-enumerable keys.
  assertEquals(["foo", "bar"], Object.keys(proxy));

  // Edge case: no properties left after filtering.
  handler.getOwnPropertyDescriptor = undefined;
  assertEquals([], Object.keys(proxy));

  // Throwing shouldn't crash.
  handler.getOwnPropertyDescriptor = function() { throw new Number(1); };
  assertThrows(() => Object.keys(proxy), Number);

  // Fall through to getOwnPropertyDescriptor if there is no trap.
  handler.ownKeys = undefined;
  assertThrows(() => Object.keys(proxy), Number);

  // Fall through to target if there is no trap.
  handler.getOwnPropertyDescriptor = undefined;
  assertEquals(["target"], Object.keys(proxy));
  assertEquals(["target"], Object.keys(target));

  var proxy2 = new Proxy(proxy, {});
  assertEquals(["target"], Object.keys(proxy2));
})();

(function testForSymbols() {
  var symbol = Symbol();
  var p = new Proxy({}, {ownKeys() { return ["1", symbol, "2"] }});
  assertEquals(["1","2"], Object.getOwnPropertyNames(p));
  assertEquals([symbol], Object.getOwnPropertySymbols(p));
})();

(function testNoProxyTraps() {
  var test_sym = Symbol("sym1");
  var test_sym2 = Symbol("sym2");
  var target = {
    one: 1,
    two: 2,
    [test_sym]: 4,
    0: 0,
  };
  Object.defineProperty(
      target, "non-enum",
      { enumerable: false, value: "nope", configurable: true, writable: true });
  target.__proto__ = {
    target_proto: 3,
    1: 1,
    [test_sym2]: 5
  };
  Object.defineProperty(
      target.__proto__, "non-enum2",
      { enumerable: false, value: "nope", configurable: true, writable: true });
  var proxy = new Proxy(target, {});

  assertEquals(["0", "one", "two"], Object.keys(proxy));
  assertEquals(["0", "one", "two", "non-enum"],
               Object.getOwnPropertyNames(proxy));
  assertEquals([test_sym], Object.getOwnPropertySymbols(proxy));
})();
