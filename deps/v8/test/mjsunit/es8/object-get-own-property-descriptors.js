// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function DataDescriptor(value) {
  return { "enumerable": true, "configurable": true, "writable": true, value };
}


function TestMeta() {
  assertEquals(1, Object.getOwnPropertyDescriptors.length);
  assertEquals(Function.prototype,
               Object.getPrototypeOf(Object.getOwnPropertyDescriptors));
  assertEquals(
      'getOwnPropertyDescriptors', Object.getOwnPropertyDescriptors.name);
  var desc = Reflect.getOwnPropertyDescriptor(
      Object, 'getOwnPropertyDescriptors');
  assertFalse(desc.enumerable);
  assertTrue(desc.writable);
  assertTrue(desc.configurable);
}
TestMeta();


function TestToObject() {
  assertThrows(function() {
    Object.getOwnPropertyDescriptors(null);
  }, TypeError);

  assertThrows(function() {
    Object.getOwnPropertyDescriptors(undefined);
  }, TypeError);

  assertThrows(function() {
    Object.getOwnPropertyDescriptors();
  }, TypeError);
}
TestToObject();


function TestPrototypeProperties() {
  function F() {};
  F.prototype.a = "A";
  F.prototype.b = "B";

  var F2 = new F();
  Object.defineProperties(F2, {
    "b": {
      enumerable: false,
      configurable: true,
      writable: false,
      value: "Shadowed 'B'"
    },
    "c": {
      enumerable: false,
      configurable: true,
      writable: false,
      value: "C"
    }
  });

  assertEquals({
    "b": {
      enumerable: false,
      configurable: true,
      writable: false,
      value: "Shadowed 'B'"
    },
    "c": {
      enumerable: false,
      configurable: true,
      writable: false,
      value: "C"
    }
  }, Object.getOwnPropertyDescriptors(F2));
}
TestPrototypeProperties();


function TestPrototypeProperties() {
  function F() {};
  F.prototype.a = "A";
  F.prototype.b = "B";

  var F2 = new F();
  Object.defineProperties(F2, {
    "b": {
      enumerable: false,
      configurable: true,
      writable: false,
      value: "Shadowed 'B'"
    },
    "c": {
      enumerable: false,
      configurable: true,
      writable: false,
      value: "C"
    }
  });

  assertEquals({
    "b": {
      enumerable: false,
      configurable: true,
      writable: false,
      value: "Shadowed 'B'"
    },
    "c": {
      enumerable: false,
      configurable: true,
      writable: false,
      value: "C"
    }
  }, Object.getOwnPropertyDescriptors(F2));
}
TestPrototypeProperties();


function TestTypeFilteringAndOrder() {
  var log = [];
  var sym = Symbol("foo");
  var psym = %CreatePrivateSymbol("private");
  var O = {
    0: 0,
    [sym]: 3,
    "a": 2,
    [psym]: 4,
    1: 1,
  };
  var P = new Proxy(O, {
    ownKeys(target) {
      log.push("ownKeys()");
      return Reflect.ownKeys(target);
    },
    getOwnPropertyDescriptor(target, name) {
      log.push(`getOwnPropertyDescriptor(${String(name)})`);
      return Reflect.getOwnPropertyDescriptor(target, name);
    },
    get(target, name) { assertUnreachable(); },
    set(target, name, value) { assertUnreachable(); },
    deleteProperty(target, name) { assertUnreachable(); },
    defineProperty(target, name, desc) { assertUnreachable(); }
  });

  var result1 = Object.getOwnPropertyDescriptors(O);
  assertEquals({
    0: DataDescriptor(0),
    1: DataDescriptor(1),
    "a": DataDescriptor(2),
    [sym]: DataDescriptor(3)
  }, result1);

  var result2 = Object.getOwnPropertyDescriptors(P);
  assertEquals([
    "ownKeys()",
    "getOwnPropertyDescriptor(0)",
    "getOwnPropertyDescriptor(1)",
    "getOwnPropertyDescriptor(a)",
    "getOwnPropertyDescriptor(Symbol(foo))"
  ], log);
  assertEquals({
    0: DataDescriptor(0),
    1: DataDescriptor(1),
    "a": DataDescriptor(2),
    [sym]: DataDescriptor(3)
  }, result2);
}
TestTypeFilteringAndOrder();


function TestDuplicateKeys() {
  var i = 0;
  var log = [];
  var P = new Proxy({}, {
    ownKeys() {
      log.push(`ownKeys()`);
      return ["A", "A"];
    },
    getOwnPropertyDescriptor(t, name) {
      log.push(`getOwnPropertyDescriptor(${name})`);
      if (i++) return;
      return {
        configurable: true,
        writable: false,
        value: "VALUE"
      };
    },
    get(target, name) { assertUnreachable(); },
    set(target, name, value) { assertUnreachable(); },
    deleteProperty(target, name) { assertUnreachable(); },
    defineProperty(target, name, desc) { assertUnreachable(); }
  });

  assertThrows(() => Object.getOwnPropertyDescriptors(P), TypeError);
}
TestDuplicateKeys();

function TestFakeProperty() {
  var log = [];
  var P = new Proxy({}, {
    ownKeys() {
      log.push(`ownKeys()`);
      return ["fakeProperty"];
    },
    getOwnPropertyDescriptor(target, name) {
      log.push(`getOwnPropertyDescriptor(${name})`);
      return;
    }
  });
  var result = Object.getOwnPropertyDescriptors(P);
  assertEquals({}, result);
  assertFalse(result.hasOwnProperty("fakeProperty"));
  assertEquals([
    "ownKeys()",
    "getOwnPropertyDescriptor(fakeProperty)"
  ], log);
}
TestFakeProperty();
