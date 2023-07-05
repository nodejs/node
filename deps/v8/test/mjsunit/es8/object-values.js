// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  assertThrows(() => Object.values(P), TypeError);
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


(function TestElementKinds() {
  var O1 = { name: "1" }, O2 = { name: "2" }, O3 = { name: "3" };
  var PI = 3.141592653589793;
  var E = 2.718281828459045;
  function fastSloppyArguments(a, b, c) {
    delete arguments[0];
    arguments[0] = a;
    return arguments;
  }

  function slowSloppyArguments(a, b, c) {
    delete arguments[0];
    arguments[0] = a;
    Object.defineProperties(arguments, {
      0: {
        enumerable: true,
        value: a
      },
      9999: {
        enumerable: false,
        value: "Y"
      }
    });
    arguments[10000] = "X";
    return arguments;
  }

  var element_kinds = {
    PACKED_SMI_ELEMENTS: [ [1, 2, 3], [1, 2, 3] ],
    HOLEY_SMI_ELEMENTS: [ [, , 3], [ 3 ] ],
    PACKED_ELEMENTS: [ [O1, O2, O3], [O1, O2, O3] ],
    HOLEY_ELEMENTS: [ [, , O3], [O3] ],
    PACKED_DOUBLE_ELEMENTS: [ [E, NaN, PI], [E, NaN, PI] ],
    HOLEY_DOUBLE_ELEMENTS: [ [, , NaN], [NaN] ],

    DICTIONARY_ELEMENTS: [ Object.defineProperties({ 10000: "world" }, {
        100: { enumerable: true, value: "hello" },
        99: { enumerable: false, value: "nope" }
      }), [ "hello", "world" ] ],
    FAST_SLOPPY_ARGUMENTS_ELEMENTS: [
        fastSloppyArguments("a", "b", "c"), ["a", "b", "c"] ],
    SLOW_SLOPPY_ARGUMENTS_ELEMENTS: [
        slowSloppyArguments("a", "b", "c"), [ "a", "b", "c", "X"]],

    FAST_STRING_WRAPPER_ELEMENTS: [ new String("str"), ["s", "t", "r"] ],
    SLOW_STRING_WRAPPER_ELEMENTS: [
        Object.defineProperties(new String("str"), {
          10000: { enumerable: false, value: "X" },
          9999: { enumerable: true, value: "Y" }
        }), ["s", "t", "r", "Y"] ],
  };

  for (let [kind, [object, expected]] of Object.entries(element_kinds)) {
    let result1 = Object.values(object);
    assertEquals(expected, result1, `fast Object.values() with ${kind}`);

    let proxy = new Proxy(object, {});
    let result2 = Object.values(proxy);
    assertEquals(result1, result2, `slow Object.values() with ${kind}`);
  }
})();


(function TestGlobalObject() {
  let values = Object.values(globalThis);
  assertTrue(values.length > 0);
})();
