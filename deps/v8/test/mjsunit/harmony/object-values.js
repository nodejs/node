// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-object-values-entries --harmony-proxies --harmony-reflect
// Flags: --allow-natives-syntax

function TestMeta() {
  assertEquals(1, Object.values.length);
  assertEquals(Function.prototype, Object.getPrototypeOf(Object.values));
  assertEquals("values", Object.values.name);

  var descriptor = Object.getOwnPropertyDescriptor(Object, "values");
  assertTrue(descriptor.writable);
  assertFalse(descriptor.enumerable);
  assertTrue(descriptor.configurable);

  assertThrows(() => new Object.values({}), TypeError);
}
TestMeta();


function TestBasic() {
  var x = 16;
  var O = {
    d: 1,
    c: 3,
    [Symbol.iterator]: void 0,
    0: 123,
    1000: 456,
    [x * x]: "ducks",
    [`0x${(x * x).toString(16)}`]: "quack"
  };
  O.a = 2;
  O.b = 4;
  Object.defineProperty(O, "HIDDEN", { enumerable: false, value: NaN });
  assertEquals([123, "ducks", 456, 1, 3, "quack", 2, 4], Object.values(O));
  assertEquals(Object.values(O), Object.keys(O).map(key => O[key]));

  assertTrue(Array.isArray(Object.values({})));
  assertEquals(0, Object.values({}).length);
}
TestBasic();


function TestToObject() {
  assertThrows(function() { Object.values(); }, TypeError);
  assertThrows(function() { Object.values(null); }, TypeError);
  assertThrows(function() { Object.values(void 0); }, TypeError);
}
TestToObject();


function TestOrder() {
  var O = {
    a: 1,
    [Symbol.iterator]: null
  };
  O[456] = 123;
  Object.defineProperty(O, "HIDDEN", { enumerable: false, value: NaN });
  var priv = %CreatePrivateSymbol("Secret");
  O[priv] = 56;

  var log = [];
  var P = new Proxy(O, {
    ownKeys(target) {
      log.push("[[OwnPropertyKeys]]");
      return Reflect.ownKeys(target);
    },
    get(target, name) {
      log.push(`[[Get]](${JSON.stringify(name)})`);
      return Reflect.get(target, name);
    },
    getOwnPropertyDescriptor(target, name) {
      log.push(`[[GetOwnProperty]](${JSON.stringify(name)})`);
      return Reflect.getOwnPropertyDescriptor(target, name);
    },
    set(target, name, value) {
      assertUnreachable();
    }
  });

  assertEquals([123, 1], Object.values(P));
  assertEquals([
    "[[OwnPropertyKeys]]",
    "[[GetOwnProperty]](\"456\")",
    "[[Get]](\"456\")",
    "[[GetOwnProperty]](\"a\")",
    "[[Get]](\"a\")",
    "[[GetOwnProperty]](\"HIDDEN\")"
  ], log);
}
TestOrder();


function TestOrderWithDuplicates() {
  var O = {
    a: 1,
    [Symbol.iterator]: null
  };
  O[456] = 123;
  Object.defineProperty(O, "HIDDEN", { enumerable: false, value: NaN });
  O[priv] = 56;
  var priv = %CreatePrivateSymbol("private");

  var log = [];
  var P = new Proxy(O, {
    ownKeys(target) {
      log.push("[[OwnPropertyKeys]]");
      return [ "a", Symbol.iterator, "a", "456", "HIDDEN", "HIDDEN", "456" ];
    },
    get(target, name) {
      log.push(`[[Get]](${JSON.stringify(name)})`);
      return Reflect.get(target, name);
    },
    getOwnPropertyDescriptor(target, name) {
      log.push(`[[GetOwnProperty]](${JSON.stringify(name)})`);
      return Reflect.getOwnPropertyDescriptor(target, name);
    },
    set(target, name, value) {
      assertUnreachable();
    }
  });

  assertEquals([1, 1, 123, 123], Object.values(P));
  assertEquals([
    "[[OwnPropertyKeys]]",
    "[[GetOwnProperty]](\"a\")",
    "[[Get]](\"a\")",
    "[[GetOwnProperty]](\"a\")",
    "[[Get]](\"a\")",
    "[[GetOwnProperty]](\"456\")",
    "[[Get]](\"456\")",
    "[[GetOwnProperty]](\"HIDDEN\")",
    "[[GetOwnProperty]](\"HIDDEN\")",
    "[[GetOwnProperty]](\"456\")",
    "[[Get]](\"456\")",
  ], log);
}
TestOrderWithDuplicates();


function TestPropertyFilter() {
  var object = { prop3: 30 };
  object[2] = 40;
  object["prop4"] = 50;
  Object.defineProperty(object, "prop5", { value: 60, enumerable: true });
  Object.defineProperty(object, "prop6", { value: 70, enumerable: false });
  Object.defineProperty(object, "prop7", {
      enumerable: true, get() { return 80; }});
  var sym = Symbol("prop8");
  object[sym] = 90;

  values = Object.values(object);
  assertEquals(5, values.length);
  assertEquals([40,30,50,60,80], values);
}
TestPropertyFilter();


function TestWithProxy() {
  var obj1 = {prop1:10};
  var proxy1 = new Proxy(obj1, { });
  assertEquals([10], Object.values(proxy1));

  var obj2 = {};
  Object.defineProperty(obj2, "prop2", { value: 20, enumerable: true });
  Object.defineProperty(obj2, "prop3", {
      get() { return 30; }, enumerable: true });
  var proxy2 = new Proxy(obj2, {
    getOwnPropertyDescriptor(target, name) {
      return Reflect.getOwnPropertyDescriptor(target, name);
    }
  });
  assertEquals([20, 30], Object.values(proxy2));

  var obj3 = {};
  var count = 0;
  var proxy3 = new Proxy(obj3, {
    get(target, property, receiver) {
      return count++ * 5;
    },
    getOwnPropertyDescriptor(target, property) {
      return { configurable: true, enumerable: true };
    },
    ownKeys(target) {
      return [ "prop0", "prop1", Symbol("prop2"), Symbol("prop5") ];
    }
  });
  assertEquals([0, 5], Object.values(proxy3));
}
TestWithProxy();


function TestMutateDuringEnumeration() {
  var aDeletesB = {
    get a() {
      delete this.b;
      return 1;
    },
    b: 2
  };
  assertEquals([1], Object.values(aDeletesB));

  var aRemovesB = {
    get a() {
      Object.defineProperty(this, "b", { enumerable: false });
      return 1;
    },
    b: 2
  };
  assertEquals([1], Object.values(aRemovesB));

  var aAddsB = { get a() { this.b = 2; return 1; } };
  assertEquals([1], Object.values(aAddsB));

  var aMakesBEnumerable = {};
  Object.defineProperty(aMakesBEnumerable, "a", {
    get() {
      Object.defineProperty(this, "b", { enumerable: true });
      return 1;
    },
    enumerable: true
  });
  Object.defineProperty(aMakesBEnumerable, "b", {
      value: 2, configurable:true, enumerable: false });
  assertEquals([1, 2], Object.values(aMakesBEnumerable));
}
TestMutateDuringEnumeration();
