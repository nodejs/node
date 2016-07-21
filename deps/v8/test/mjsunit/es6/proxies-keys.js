// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
assertThrows("Object.keys(proxy)", Number);

// Fall through to target if there is no trap.
handler.ownKeys = undefined;
assertEquals(["target"], Object.keys(proxy));
assertEquals(["target"], Object.keys(target));

var proxy2 = new Proxy(proxy, {});
assertEquals(["target"], Object.keys(proxy2));


(function testForSymbols() {
  var symbol = Symbol();
  var p = new Proxy({}, {ownKeys() { return ["1", symbol, "2"] }});
  assertEquals(["1","2"], Object.getOwnPropertyNames(p));
  assertEquals([symbol], Object.getOwnPropertySymbols(p));
})();
