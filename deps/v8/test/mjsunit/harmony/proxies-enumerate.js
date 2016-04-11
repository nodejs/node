// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-proxies

var target = {
  "target_one": 1
};
target.__proto__ = {
  "target_two": 2
};
var handler = {
  enumerate: function(target) {
    function* keys() {
      yield "foo";
      yield "bar";
    }
    return keys();
  },
  // For-in calls "has" on every iteration, so for TestForIn() below to
  // detect all results of the "enumerate" trap, "has" must return true.
  has: function(target, name) {
    return true;
  }
}

var proxy = new Proxy(target, handler);

function TestForIn(receiver, expected) {
  var result = [];
  for (var k in receiver) {
    result.push(k);
  }
  assertEquals(expected, result);
}

TestForIn(proxy, ["foo", "bar"]);

// Test revoked proxy.
var pair = Proxy.revocable(target, handler);
TestForIn(pair.proxy, ["foo", "bar"]);
pair.revoke();
assertThrows(()=>{ TestForIn(pair.proxy, ["foo", "bar"]) }, TypeError);

// Properly call traps on proxies on the prototype chain.
var receiver = {
  "receiver_one": 1
};
receiver.__proto__ = proxy;
TestForIn(receiver, ["receiver_one", "foo", "bar"]);

// Fall through to default behavior when trap is undefined.
handler.enumerate = undefined;
TestForIn(proxy, ["target_one", "target_two"]);
delete handler.enumerate;
TestForIn(proxy, ["target_one", "target_two"]);

// Non-string keys must be filtered.
function TestNonStringKey(key) {
  handler.enumerate = function(target) {
    function* keys() { yield key; }
    return keys();
  }
  assertThrows("for (var k in proxy) {}", TypeError);
}

TestNonStringKey(1);
TestNonStringKey(3.14);
TestNonStringKey(Symbol("foo"));
TestNonStringKey({bad: "value"});
TestNonStringKey(null);
TestNonStringKey(undefined);
TestNonStringKey(true);

(function testProtoProxyEnumerate() {
  var keys = ['a', 'b', 'c', 'd'];
  var handler = {
   enumerate() { return keys[Symbol.iterator]() },
   has(target, key) { return false }
  };
  var proxy = new Proxy({}, handler);
  var seen_keys = [];
  for (var i in proxy) {
    seen_keys.push(i);
  }
  assertEquals([], seen_keys);

  handler.has = function(target, key) { return true };
  for (var i in proxy) {
    seen_keys.push(i);
  }
  assertEquals(keys, seen_keys);

  o = {__proto__:proxy};
  handler.has = function(target, key) { return false };
  seen_keys = [];
  for (var i in o) {
    seen_keys.push(i);
  }
  assertEquals([], seen_keys);

  handler.has = function(target, key) { return true };
  seen_keys = [];
  for (var i in o) {
    seen_keys.push(i);
  }
  assertEquals(keys, seen_keys);
})();
