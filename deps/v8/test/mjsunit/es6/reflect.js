// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(neis): Test with proxies.



////////////////////////////////////////////////////////////////////////////////
// (Auxiliaries)


"use strict";

var global = this;

var sym = Symbol("gaga");

var objects = [
  {},
  [],
  function() {},
  function() {
    return arguments;
  }(),
  function() {
    'use strict';
    return arguments;
  }(),
  Object(1),
  Object(true),
  Object('bla'),
  new Date,
  new RegExp,
  new Set,
  new Map,
  new WeakMap,
  new WeakSet,
  new ArrayBuffer(10),
  new Int32Array(5),
  Object,
  Function,
  Date,
  RegExp,
  global
];

function prepare(target) {
  target["bla"] = true;
  target[4] = 42;
  target[sym] = "foo";
  target["noconf"] = 43;
  Object.defineProperty(target, "noconf",
      { configurable: false });
  Object.defineProperty(target, "nowrite",
      { writable: false, configurable: true, value: 44 });
  Object.defineProperty(target, "getter",
      { get: function () {return this.bla}, configurable: true });
  Object.defineProperty(target, "setter",
      { set: function (x) {this.gaga = x}, configurable: true });
  Object.defineProperty(target, "setter2",
      { set: function (x) {}, configurable: true });
}



////////////////////////////////////////////////////////////////////////////////
// Reflect.get


(function testReflectGetArity() {
  assertEquals(2, Reflect.get.length);
})();


(function testReflectGetOnNonObject() {
  assertThrows(function() { Reflect.get(); }, TypeError);
  assertThrows(function() { Reflect.get(42, "bla"); }, TypeError);
  assertThrows(function() { Reflect.get(null, "bla"); }, TypeError);
})();


(function testReflectGetKeyConversion() {
  var target = {bla: 42};
  var a = { [Symbol.toPrimitive]: function() { return "bla" } };
  var b = { [Symbol.toPrimitive]: function() { throw "gaga" } };
  assertEquals(42, Reflect.get(target, a));
  assertThrowsEquals(function() { Reflect.get(target, b); }, "gaga");
})();


(function testReflectGetOnObject() {
  var receiver = {bla: false};
  for (let target of objects) {
    prepare(target);
    assertEquals(true, Reflect.get(target, "bla"));
    assertEquals(true, Reflect.get(target, "bla", target));
    assertEquals(true, Reflect.get(target, "bla", receiver));
    assertEquals(42, Reflect.get(target, 4));
    assertEquals(42, Reflect.get(target, 4, target));
    assertEquals(42, Reflect.get(target, 4, receiver));
    assertEquals(42, Reflect.get(target, "4"));
    assertEquals(42, Reflect.get(target, "4", target));
    assertEquals(42, Reflect.get(target, "4", receiver));
    assertEquals("foo", Reflect.get(target, sym));
    assertEquals("foo", Reflect.get(target, sym, target));
    assertEquals("foo", Reflect.get(target, sym, receiver));
    assertEquals(43, Reflect.get(target, "noconf"));
    assertEquals(43, Reflect.get(target, "noconf", target));
    assertEquals(43, Reflect.get(target, "noconf", receiver));
    assertEquals(true, Reflect.get(target, "getter"));
    assertEquals(true, Reflect.get(target, "getter", target));
    assertEquals(false, Reflect.get(target, "getter", receiver));
    assertEquals(undefined, Reflect.get(target, "setter"));
    assertEquals(undefined, Reflect.get(target, "setter", target));
    assertEquals(undefined, Reflect.get(target, "setter", receiver));
    assertEquals(undefined, Reflect.get(target, "foo"));
    assertEquals(undefined, Reflect.get(target, "foo", target));
    assertEquals(undefined, Reflect.get(target, "foo", receiver));
    assertEquals(undefined, Reflect.get(target, 666));
    assertEquals(undefined, Reflect.get(target, 666, target));
    assertEquals(undefined, Reflect.get(target, 666, receiver));

    let proto = target.__proto__;
    target.__proto__ = { get foo() {return this.bla} };
    assertEquals(true, Reflect.get(target, "foo"));
    assertEquals(true, Reflect.get(target, "foo", target));
    assertEquals(false, Reflect.get(target, "foo", receiver));
    target.__proto__ = proto;
  }
})();



////////////////////////////////////////////////////////////////////////////////
// Reflect.set


(function testReflectSetArity() {
  assertEquals(3, Reflect.set.length);
})();


(function testReflectSetOnNonObject() {
  assertThrows(function() { Reflect.set(); }, TypeError);
  assertThrows(function() { Reflect.set(42, "bla"); }, TypeError);
  assertThrows(function() { Reflect.set(null, "bla"); }, TypeError);
})();


(function testReflectSetKeyConversion() {
  var target = {};
  var a = { [Symbol.toPrimitive]: function() { return "bla" } };
  var b = { [Symbol.toPrimitive]: function() { throw "gaga" } };
  assertTrue(Reflect.set(target, a, 42));
  assertEquals(42, target.bla);
  assertThrowsEquals(function() { Reflect.set(target, b, 42); }, "gaga");
})();


(function testReflectSetOnObject() {
  var receiver = {bla: false};
  var value = 34234;
  for (let target of objects) {
    prepare(target);
    assertTrue(Reflect.set(target, "bla", value));
    assertEquals(value, target.bla);

    prepare(target);
    assertTrue(Reflect.set(target, "bla", value, target));
    assertEquals(value, target.bla);

    prepare(target);
    assertTrue(Reflect.set(target, "bla", value, receiver));
    assertEquals(true, target.bla);
    assertEquals(value, receiver.bla);
    receiver.bla = false;

    prepare(target);
    assertTrue(Reflect.set(target, 4, value));
    assertEquals(value, target[4]);

    prepare(target);
    assertTrue(Reflect.set(target, 4, value, target));
    assertEquals(value, target[4]);

    prepare(target);
    assertTrue(Reflect.set(target, 4, value, receiver));
    assertEquals(42, target[4]);
    assertEquals(value, receiver[4]);
    delete receiver[4];

    prepare(target);
    assertTrue(Reflect.set(target, sym, value));
    assertEquals(value, target[sym]);

    prepare(target);
    assertTrue(Reflect.set(target, sym, value, target));
    assertEquals(value, target[sym]);

    prepare(target);
    assertTrue(Reflect.set(target, sym, value, receiver));
    assertEquals("foo", target[sym]);
    assertEquals(value, receiver[sym]);
    delete receiver[sym];

    prepare(target);
    assertTrue(Reflect.set(target, "noconf", value));
    assertEquals(value, target.noconf);

    prepare(target);
    assertTrue(Reflect.set(target, "noconf", value, target));
    assertEquals(value, target.noconf);

    prepare(target);
    assertTrue(Reflect.set(target, "noconf", value, receiver));
    assertEquals(43, target.noconf);
    assertEquals(value, receiver.noconf);
    delete receiver.noconf;

    assertTrue(Reflect.set(target, "setter", value));
    assertEquals(value, target.gaga)
    delete target.gaga;

    assertTrue(Reflect.set(target, "setter", value, target));
    assertEquals(value, target.gaga)
    delete target.gaga;

    assertTrue(Reflect.set(target, "setter", value, receiver));
    assertFalse("gaga" in target);
    assertEquals(value, receiver.gaga);
    delete receiver.gaga;

    assertFalse(Reflect.set(target, "nowrite", value));
    assertEquals(44, target.nowrite);

    assertFalse(Reflect.set(target, "nowrite", value, target));
    assertEquals(44, target.nowrite);

    assertFalse(Reflect.set(target, "nowrite", value, receiver));
    assertEquals(44, target.nowrite);
    assertFalse("nowrite" in receiver);

    // Data vs Non-Writable
    assertFalse(Reflect.set({}, "nowrite", value, target));

    // Data vs Accessor
    assertFalse(Reflect.set({}, "unknown", 0, {set unknown(x) {}}));
    assertFalse(Reflect.set(target, "unknown", value, {set unknown(x) {}}));
    assertFalse(Reflect.set(target, "bla", value, {set bla(x) {}}));
    assertFalse(Reflect.set(target, "bla", value, {get bla() {}}));

    // Accessor vs Data
    assertTrue(Reflect.set({set bla(x) {}}), "bla", value, target);
    assertFalse(Reflect.set({get bla() {}}, "bla", value, target));

    // Data vs Non-Object
    assertFalse(Reflect.set({}, "bla", value, null));
    assertFalse(Reflect.set({bla: 42}, "bla", value, null));

    // Accessor vs Non-Object
    assertTrue(Reflect.set(target, "setter2", value, null));
    assertFalse(Reflect.set(target, "getter", value, null));

    let receiver2 = {};
    Object.defineProperty(receiver2, "bla",
        {configurable: false, writable: true, value: true});
    Object.defineProperty(receiver2, "not_in_target",
        {configurable: false, writable: true, value: true});
    assertTrue(Reflect.set(target, "bla", value, receiver2));
    assertTrue(Reflect.set(target, "not_in_target", value, receiver2));
  }
})();



////////////////////////////////////////////////////////////////////////////////
// Reflect.has


(function testReflectHasArity() {
  assertEquals(2, Reflect.has.length);
})();


(function testReflectHasOnNonObject() {
  assertThrows(function() { Reflect.has(); }, TypeError);
  assertThrows(function() { Reflect.has(42, "bla"); }, TypeError);
  assertThrows(function() { Reflect.has(null, "bla"); }, TypeError);
})();


(function testReflectHasKeyConversion() {
  var target = {bla: 42};
  var a = { [Symbol.toPrimitive]: function() { return "bla" } };
  var b = { [Symbol.toPrimitive]: function() { throw "gaga" } };
  assertTrue(Reflect.has(target, a));
  assertThrowsEquals(function() { Reflect.has(target, b); }, "gaga");
})();


(function testReflectHasOnObject() {
  for (let target of objects) {
    prepare(target);
    assertTrue(Reflect.has(target, "bla"));
    assertTrue(Reflect.has(target, 4));
    assertTrue(Reflect.has(target, "4"));
    assertTrue(Reflect.has(target, sym));
    assertTrue(Reflect.has(target, "noconf"));
    assertTrue(Reflect.has(target, "getter"));
    assertTrue(Reflect.has(target, "setter"));
    assertFalse(Reflect.has(target, "foo"));
    assertFalse(Reflect.has(target, 666));

    let proto = target.__proto__;
    target.__proto__ = { get foo() {return this.bla} };
    assertEquals(true, Reflect.has(target, "foo"));
    target.__proto__ = proto;
  }
})();



////////////////////////////////////////////////////////////////////////////////
// Reflect.defineProperty


(function testReflectDefinePropertyArity() {
  assertEquals(3, Reflect.defineProperty.length);
})();


(function testReflectDefinePropertyOnNonObject() {
  assertThrows(function() { Reflect.defineProperty(); }, TypeError);
  assertThrows(function() { Reflect.defineProperty(42, "bla"); }, TypeError);
  assertThrows(function() { Reflect.defineProperty(null, "bla"); }, TypeError);
  assertThrows(function() { Reflect.defineProperty({}, "bla"); }, TypeError);
  assertThrows(function() { Reflect.defineProperty({}, "bla", 42); },
      TypeError);
  assertThrows(function() { Reflect.defineProperty({}, "bla", null); },
      TypeError);
})();


(function testReflectDefinePropertyKeyConversion() {
  var target = {};
  var a = { [Symbol.toPrimitive]: function() { return "bla" } };
  var b = { [Symbol.toPrimitive]: function() { throw "gaga" } };
  assertTrue(Reflect.defineProperty(target, a, {value: 42}));
  assertEquals(target.bla, 42);
  assertThrowsEquals(function() { Reflect.defineProperty(target, b); }, "gaga");
})();


// See reflect-define-property.js for further tests.



////////////////////////////////////////////////////////////////////////////////
// Reflect.deleteProperty


(function testReflectDeletePropertyArity() {
  assertEquals(2, Reflect.deleteProperty.length);
})();


(function testReflectDeletePropertyOnNonObject() {
  assertThrows(function() { Reflect.deleteProperty(); }, TypeError);
  assertThrows(function() { Reflect.deleteProperty(42, "bla"); }, TypeError);
  assertThrows(function() { Reflect.deleteProperty(null, "bla"); }, TypeError);
})();


(function testReflectDeletePropertyKeyConversion() {
  var target = {bla: 42};
  var a = { [Symbol.toPrimitive]: function() { return "bla" } };
  var b = { [Symbol.toPrimitive]: function() { throw "gaga" } };
  assertTrue(Reflect.deleteProperty(target, a));
  assertThrowsEquals(function() { Reflect.deleteProperty(target, b); }, "gaga");
})();


(function testReflectDeletePropertyOnObject() {
  for (let target of objects) {
    prepare(target);
    assertTrue(Reflect.deleteProperty(target, "bla"));
    assertEquals(undefined, Object.getOwnPropertyDescriptor(target, "bla"));
    if (target instanceof Int32Array) {
      assertFalse(Reflect.deleteProperty(target, 4));
    } else {
      assertTrue(Reflect.deleteProperty(target, 4));
      assertEquals(undefined, Object.getOwnPropertyDescriptor(target, 4));
    }
    assertTrue(Reflect.deleteProperty(target, sym));
    assertEquals(undefined, Object.getOwnPropertyDescriptor(target, sym));
    assertFalse(Reflect.deleteProperty(target, "noconf"));
    assertEquals(43, target.noconf);
    assertTrue(Reflect.deleteProperty(target, "getter"));
    assertTrue(Reflect.deleteProperty(target, "setter"));
    assertTrue(Reflect.deleteProperty(target, "foo"));
    assertTrue(Reflect.deleteProperty(target, 666));

    let proto = target.__proto__;
    target.__proto__ = { get foo() {return this.bla} };
    assertEquals(true, Reflect.deleteProperty(target, "foo"));
    target.__proto__ = proto;
  }
})();



////////////////////////////////////////////////////////////////////////////////
// Reflect.getPrototypeOf


(function testReflectGetPrototypeOfArity() {
  assertEquals(1, Reflect.getPrototypeOf.length);
})();


(function testReflectGetPrototypeOnNonObject() {
  assertThrows(function() { Reflect.getPrototypeOf(); }, TypeError);
  assertThrows(function() { Reflect.getPrototypeOf(42); }, TypeError);
  assertThrows(function() { Reflect.getPrototypeOf(null); }, TypeError);
})();


// See reflect-get-prototype-of.js for further tests.



////////////////////////////////////////////////////////////////////////////////
// Reflect.setPrototypeOf


(function testReflectSetPrototypeOfArity() {
  assertEquals(2, Reflect.setPrototypeOf.length);
})();


(function testReflectSetPrototypeOfOnNonObject() {
  assertThrows(function() { Reflect.setPrototypeOf(undefined, {}); },
      TypeError);
  assertThrows(function() { Reflect.setPrototypeOf(42, {}); }, TypeError);
  assertThrows(function() { Reflect.setPrototypeOf(null, {}); }, TypeError);

  assertThrows(function() { Reflect.setPrototypeOf({}, undefined); },
      TypeError);
  assertThrows(function() { Reflect.setPrototypeOf({}, 42); }, TypeError);
  assertTrue(Reflect.setPrototypeOf({}, null));
})();


// See reflect-set-prototype-of.js for further tests.



////////////////////////////////////////////////////////////////////////////////
// Reflect.isExtensible


(function testReflectIsExtensibleArity() {
  assertEquals(1, Reflect.isExtensible.length);
})();


(function testReflectIsExtensibleOnNonObject() {
  assertThrows(function() { Reflect.isExtensible(); }, TypeError);
  assertThrows(function() { Reflect.isExtensible(42); }, TypeError);
  assertThrows(function() { Reflect.isExtensible(null); }, TypeError);
})();


(function testReflectIsExtensibleOnObject() {
  // This should be the last test on [objects] as it modifies them irreversibly.
  for (let target of objects) {
    prepare(target);
    if (target instanceof Int32Array) continue;  // issue v8:4460
    assertTrue(Reflect.isExtensible(target));
    Object.preventExtensions(target);
    assertFalse(Reflect.isExtensible(target));
  }
})();


////////////////////////////////////////////////////////////////////////////////
// Reflect.getOwnPropertyDescriptor


(function testReflectGetOwnPropertyDescriptorArity() {
  assertEquals(2, Reflect.getOwnPropertyDescriptor.length);
})();


(function testReflectGetOwnPropertyDescriptorOnNonObject() {
  assertThrows(function() { Reflect.getOwnPropertyDescriptor(); }, TypeError);
  assertThrows(function() { Reflect.getOwnPropertyDescriptor(42); },
      TypeError);
  assertThrows(function() { Reflect.getOwnPropertyDescriptor(null); },
      TypeError);
})();


(function testReflectGetOwnPropertyDescriptorKeyConversion() {
  var target = {bla: 42};
  var a = { [Symbol.toPrimitive]: function() { return "bla" } };
  var b = { [Symbol.toPrimitive]: function() { throw "gaga" } };
  assertEquals(42, Reflect.getOwnPropertyDescriptor(target, a).value);
  assertThrowsEquals(() => Reflect.getOwnPropertyDescriptor(target, b), "gaga");
})();


// See reflect-get-own-property-descriptor.js for further tests.



////////////////////////////////////////////////////////////////////////////////
// Reflect.ownKeys


(function testReflectOwnKeysArity() {
  assertEquals(1, Reflect.ownKeys.length);
})();


(function testReflectOwnKeysOnNonObject() {
  assertThrows(function() { Reflect.ownKeys(); }, TypeError);
  assertThrows(function() { Reflect.ownKeys(42); }, TypeError);
  assertThrows(function() { Reflect.ownKeys(null); }, TypeError);
})();


(function testReflectOwnKeysOnObject(){
  assertEquals(["z", "y", "x"], Reflect.ownKeys({z: 3, y: 2, x: 1}));
  assertEquals(["length"], Reflect.ownKeys([]));

  var s1 = Symbol("foo");
  var s2 = Symbol("bar");
  var obj = { [s1]: 0, "bla": 0, 42: 0, "0": 0,
      [s2]: 0, "-1": 0, "88": 0, "aaa": 0 };
  assertEquals(["0", "42", "88", "bla", "-1", "aaa", s1, s2],
      Reflect.ownKeys(obj));
  // Force dict-mode elements.
  delete obj[0];
  assertEquals(["42", "88", "bla", "-1", "aaa", s1, s2],
      Reflect.ownKeys(obj));
  // Force dict-mode properties.
  delete obj["bla"];
  assertEquals(["42", "88", "-1", "aaa", s1, s2], Reflect.ownKeys(obj));
})();


// See reflect-own-keys.js for further tests.



////////////////////////////////////////////////////////////////////////////////
// Reflect.preventExtensions


(function testReflectPreventExtensionsArity() {
  assertEquals(1, Reflect.preventExtensions.length);
})();


(function testReflectPreventExtensionsOnNonObject() {
  assertThrows(function() { Reflect.preventExtensions(); }, TypeError);
  assertThrows(function() { Reflect.preventExtensions(42); }, TypeError);
  assertThrows(function() { Reflect.preventExtensions(null); }, TypeError);
})();


// See reflect-prevent-extensions.js for further tests.

// TODO(neis): Need proxies to test the situation where
// [[preventExtensions]] returns false.
