// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testNewTarget() {
  assertThrows(function(){ Proxy({}, {}); }, TypeError);
  assertDoesNotThrow(function(){ new Proxy({}, {}); });
})();

(function testNonObjectTargetTypes() {
  assertThrows(function(){ new Proxy(undefined, {}); }, TypeError);

  assertThrows(function(){ new Proxy(null, {}); }, TypeError);

  assertThrows(function(){ new Proxy('', {}); }, TypeError);

  assertThrows(function(){ new Proxy(0, {}); }, TypeError);

  assertThrows(function(){ new Proxy(0.5, {}); }, TypeError);

  assertThrows(function(){ new Proxy(false, {}); }, TypeError);
})();


(function testRevokedTarget() {
  var revocable = Proxy.revocable({}, {});
  revocable.revoke();

  assertThrows(function(){ new Proxy(revocable.proxy, {}); }, TypeError);
})();


(function testNonObjectHandlerTypes() {
  assertThrows(function(){ new Proxy({}, undefined); }, TypeError);

  assertThrows(function(){ new Proxy({}, null); }, TypeError);

  assertThrows(function(){ new Proxy({}, ''); }, TypeError);

  assertThrows(function(){ new Proxy({}, 0); }, TypeError);

  assertThrows(function(){ new Proxy({}, 0.5); }, TypeError);

  assertThrows(function(){ new Proxy({}, false); }, TypeError);
})();


(function testRevokedHandler() {
  var revocable = Proxy.revocable({}, {});
  revocable.revoke();

  assertThrows(function(){ new Proxy({}, revocable.proxy); }, TypeError);
})();


(function testConstructionWithoutArguments() {
  assertThrows(function(){ new Proxy(); }, TypeError);

  assertThrows(function(){ new Proxy(42); }, TypeError);

  assertThrows(function(){ new Proxy({}); }, TypeError);
})();


(function testConstructionFromArray() {
  var p = new Proxy([42], {});
  assertTrue(p instanceof Array);
  assertEquals(p[0], 42);
})();


(function testConstructionFromObject() {
  var p = new Proxy({
    prop: 42
  }, {});
  assertTrue(p instanceof Object);
  assertEquals(p.prop, 42);
})();


(function testConstructionFromCallable() {
  var p = new Proxy(() => { return 42; }, {});
  assertTrue(p instanceof Function);
  assertEquals(p(), 42);
})();


(function testConstructionFromConstructor() {
  class foo {};
  var p = new Proxy(foo, {});
  assertTrue(p instanceof Function);
  assertTrue(new p() instanceof foo);
})();


(function testConstructionFromProxy() {
  var q = new Proxy({}, {});
  var p = new Proxy(q, {});
  assertTrue(p instanceof Object);
})();
