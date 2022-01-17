// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on Mozilla Object.assign() tests

// Flags: --allow-natives-syntax

function checkDataProperty(object, propertyKey, value, writable, enumerable, configurable) {
  var desc = Object.getOwnPropertyDescriptor(object, propertyKey);
  assertFalse(desc === undefined);
  assertTrue('value' in desc);
  assertEquals(desc.value, value);
  assertEquals(desc.writable, writable);
  assertEquals(desc.enumerable, enumerable);
  assertEquals(desc.configurable, configurable);
}

// 19.1.2.1 Object.assign ( target, ...sources )
assertEquals(Object.assign.length, 2);

// Basic functionality works with multiple sources
(function basicMultipleSources() {
  var a = {};
  var b = { bProp: 1 };
  var c = { cProp: 2 };
  Object.assign(a, b, c);
  assertEquals(a, {
    bProp: 1,
    cProp: 2
  });
})();

// Basic functionality works with symbols
(function basicSymbols() {
  var a = {};
  var b = { bProp: 1 };
  var aSymbol = Symbol("aSymbol");
  b[aSymbol] = 2;
  Object.assign(a, b);
  assertEquals(1, a.bProp);
  assertEquals(2, a[aSymbol]);
})();

// Dies if target is null or undefined
assertThrows(function() { return Object.assign(null, null); }, TypeError);
assertThrows(function() { return Object.assign(null, {}); }, TypeError);
assertThrows(function() { return Object.assign(undefined); }, TypeError);
assertThrows(function() { return Object.assign(); }, TypeError);

// Calls ToObject for target
assertTrue(Object.assign(true, {}) instanceof Boolean);
assertTrue(Object.assign(1, {}) instanceof Number);
assertTrue(Object.assign("string", {}) instanceof String);
var o = {};
assertSame(Object.assign(o, {}), o);

// Only [[Enumerable]] properties are assigned to target
(function onlyEnumerablePropertiesAssigned() {
  var source = Object.defineProperties({}, {
    a: {value: 1, enumerable: true},
    b: {value: 2, enumerable: false},
  });
  var target = Object.assign({}, source);
  assertTrue("a" in target);
  assertFalse("b" in target);
})();

// Properties are retrieved through Get()
// Properties are assigned through Put()
(function testPropertiesAssignedThroughPut() {
  var setterCalled = false;
  Object.assign({set a(v) { setterCalled = v }}, {a: true});
  assertTrue(setterCalled);
})();

// Properties are retrieved through Get()
// Properties are assigned through Put(): Existing property attributes are not altered
(function propertiesAssignedExistingNotAltered() {
  var source = {a: 1, b: 2, c: 3};
  var target = {a: 0, b: 0, c: 0};
  Object.defineProperty(target, "a", {enumerable: false});
  Object.defineProperty(target, "b", {configurable: false});
  Object.defineProperty(target, "c", {enumerable: false, configurable: false});
  Object.assign(target, source);
  checkDataProperty(target, "a", 1, true, false, true);
  checkDataProperty(target, "b", 2, true, true, false);
  checkDataProperty(target, "c", 3, true, false, false);
})();

// Properties are retrieved through Get()
// Properties are assigned through Put(): Throws TypeError if non-writable
(function propertiesAssignedTypeErrorNonWritable() {
  var source = {a: 1};
  var target = {a: 0};
  Object.defineProperty(target, "a", {writable: false});
  assertThrows(function() { return Object.assign(target, source); }, TypeError);
  checkDataProperty(target, "a", 0, false, true, true);
})();

// Properties are retrieved through Get()
// Put() creates standard properties; Property attributes from source are
// ignored
(function createsStandardProperties() {
  var source = {a: 1, b: 2, c: 3, get d() { return 4 }};
  Object.defineProperty(source, "b", {writable: false});
  Object.defineProperty(source, "c", {configurable: false});
  var target = Object.assign({}, source);
  checkDataProperty(target, "a", 1, true, true, true);
  checkDataProperty(target, "b", 2, true, true, true);
  checkDataProperty(target, "c", 3, true, true, true);
  checkDataProperty(target, "d", 4, true, true, true);
})();

// Properties created during traversal are not copied
(function propertiesCreatedDuringTraversalNotCopied() {
  var source = {get a() { this.b = 2 }};
  var target = Object.assign({}, source);
  assertTrue("a" in target);
  assertFalse("b" in target);
})();

// String and Symbol valued properties are copied
(function testStringAndSymbolPropertiesCopied() {
  var keyA = "str-prop";
  var source = {"str-prop": 1};
  var target = Object.assign({}, source);
  checkDataProperty(target, keyA, 1, true, true, true);
})();

(function testExceptionsStopFirstException() {
  var ErrorA = function ErrorA() {};
  var ErrorB = function ErrorB() {};
  var log = "";
  var source = { b: 1, a: 1 };
  var target = {
      set a(v) { log += "a"; throw new ErrorA },
      set b(v) { log += "b"; throw new ErrorB },
  };
  assertThrows(function() { return Object.assign(target, source); }, ErrorB);
  assertEquals(log, "b");
})();

(function add_to_source() {
  var target = {set k1(v) { source.k3 = 100; }};
  var source = {k1:10};
  Object.defineProperty(source, "k2",
      {value: 20, enumerable: false, configurable: true});
  Object.assign(target, source);
  assertEquals(undefined, target.k2);
  assertEquals(undefined, target.k3);
})();

(function reconfigure_enumerable_source() {
  var target = {set k1(v) {
    Object.defineProperty(source, "k2", {value: 20, enumerable: true});
  }};
  var source = {k1:10};
  Object.defineProperty(source, "k2",
      {value: 20, enumerable: false, configurable: true});
  Object.assign(target, source);
  assertEquals(20, target.k2);
})();

(function propagate_assign_failure() {
  var target = {set k1(v) { throw "fail" }};
  var source = {k1:10};
  assertThrows(()=>Object.assign(target, source));
})();

(function propagate_read_failure() {
  var target = {};
  var source = {get k1() { throw "fail" }};
  assertThrows(()=>Object.assign(target, source));
})();

(function strings_and_symbol_order1() {
  // first order
  var log = [];

  var sym1 = Symbol("x"), sym2 = Symbol("y");
  var source = {
      get [sym1](){ log.push("get sym1"); },
      get a() { log.push("get a"); },
      get b() { log.push("get b"); },
      get c() { log.push("get c"); },
      get [sym2](){ log.push("get sym2"); },
  };

  Object.assign({}, source);

  assertEquals(log, ["get a", "get b", "get c", "get sym1", "get sym2"]);
})();

(function strings_and_symbol_order2() {
  // first order
  var log = [];

  var sym1 = Symbol("x"), sym2 = Symbol("y");
  var source = {
      get [sym1](){ log.push("get sym1"); },
      get a() { log.push("get a"); },
      get [sym2](){ log.push("get sym2"); },
      get b() { log.push("get b"); },
      get c() { log.push("get c"); },
  };

  Object.assign({}, source);

  assertEquals(log, ["get a", "get b", "get c", "get sym1", "get sym2"]);
})();


(function strings_and_symbol_order3() {
  // first order
  var log = [];

  var sym1 = Symbol("x"), sym2 = Symbol("y");
  var source = {
      get a() { log.push("get a"); },
      get [sym1](){ log.push("get sym1"); },
      get b() { log.push("get b"); },
      get [sym2](){ log.push("get sym2"); },
      get c() { log.push("get c"); },
  };

  Object.assign({}, source);

  assertEquals(log, ["get a", "get b", "get c", "get sym1", "get sym2"]);
})();

(function proxy() {
  const fast_source = { key1: "value1", key2: "value2"};
  const slow_source = {__proto__:null};
  for (let i = 0; i < 2000; i++) {
    slow_source["key" + i] = i;
  }

  const empty_handler = {};
  let target = {};
  let proxy = new Proxy(target, empty_handler);
  assertArrayEquals(Object.keys(target), []);
  let result = Object.assign(proxy, fast_source);
  %HeapObjectVerify(result);
  assertArrayEquals(Object.keys(result), Object.keys(target));
  assertArrayEquals(Object.keys(result), Object.keys(fast_source));
  assertArrayEquals(Object.values(result), Object.values(fast_source));

  target = {};
  proxy = new Proxy(target, empty_handler);
  assertArrayEquals(Object.keys(target), []);
  result = Object.assign(proxy, slow_source);
  %HeapObjectVerify(result);
  assertEquals(Object.keys(result).length, Object.keys(target).length);
  assertEquals(Object.keys(result).length, Object.keys(slow_source).length);
})();

(function global_object() {
  let source = {
    global1: "global1",
    get global2() { return "global2" },
  };
  let result = Object.assign(globalThis, source);
  %HeapObjectVerify(result);
  assertTrue(result === globalThis);
  assertTrue(result.global1 === source.global1);
  assertTrue(result.global2 === source.global2);

  let target = {};
  result = Object.assign(target, globalThis);
  %HeapObjectVerify(result);
  assertTrue(result === target);
  assertTrue(result.global1 === source.global1);
  assertTrue(result.global2 === source.global2);

  for (let i = 0; i < 2000; i++) {
    source["property" + i] = i;
  }
  result = Object.assign(globalThis, source);
  %HeapObjectVerify(result);
  assertTrue(result === globalThis);
  for (let i = 0; i < 2000; i++) {
    const key = "property" + i;
    assertEquals(result[key], i);
  }

})();
