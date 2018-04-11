// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function TestMeta() {
  assertEquals(1, Object.entries.length);
  assertEquals(Function.prototype, Object.getPrototypeOf(Object.entries));
  assertEquals("entries", Object.entries.name);

  var descriptor = Object.getOwnPropertyDescriptor(Object, "entries");
  assertTrue(descriptor.writable);
  assertFalse(descriptor.enumerable);
  assertTrue(descriptor.configurable);

  assertThrows(() => new Object.entries({}), TypeError);
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
  assertEquals([
    ["0", 123],
    ["256", "ducks"],
    ["1000", 456],
    ["d", 1],
    ["c", 3],
    ["0x100", "quack"],
    ["a", 2],
    ["b", 4]
  ], Object.entries(O));
  assertEquals(Object.entries(O), Object.keys(O).map(key => [key, O[key]]));

  assertTrue(Array.isArray(Object.entries({})));
  assertEquals(0, Object.entries({}).length);
}
TestBasic();


function TestToObject() {
  assertThrows(function() { Object.entries(); }, TypeError);
  assertThrows(function() { Object.entries(null); }, TypeError);
  assertThrows(function() { Object.entries(void 0); }, TypeError);
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

  assertEquals([["456", 123], ["a", 1]], Object.entries(P));
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
  var priv = %CreatePrivateSymbol("Secret");
  O[priv] = 56;

  var log = [];
  var P = new Proxy(O, {
    ownKeys(target) {
      log.push("[[OwnPropertyKeys]]");
      return ["a", Symbol.iterator, "a", "456", "HIDDEN", "HIDDEN", "456"];
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

  assertEquals([
    ["a", 1],
    ["a", 1],
    ["456", 123],
    ["456", 123]
  ], Object.entries(P));
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
    "[[Get]](\"456\")"
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

  values = Object.entries(object);
  assertEquals(5, values.length);
  assertEquals([
    [ "2", 40 ],
    [ "prop3", 30 ],
    [ "prop4", 50 ],
    [ "prop5", 60 ],
    [ "prop7", 80 ]
  ], values);
}
TestPropertyFilter();


function TestWithProxy() {
  var obj1 = {prop1:10};
  var proxy1 = new Proxy(obj1, { });
  assertEquals([ [ "prop1", 10 ] ], Object.entries(proxy1));

  var obj2 = {};
  Object.defineProperty(obj2, "prop2", { value: 20, enumerable: true });
  Object.defineProperty(obj2, "prop3", {
      get() { return 30; }, enumerable: true });
  var proxy2 = new Proxy(obj2, {
    getOwnPropertyDescriptor(target, name) {
      return Reflect.getOwnPropertyDescriptor(target, name);
    }
  });
  assertEquals([ [ "prop2", 20 ], [ "prop3", 30 ] ], Object.entries(proxy2));

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
  assertEquals([ [ "prop0", 0 ], [ "prop1", 5 ] ], Object.entries(proxy3));
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
  assertEquals([ [ "a", 1 ] ], Object.entries(aDeletesB));

  var aRemovesB = {
    get a() {
      Object.defineProperty(this, "b", { enumerable: false });
      return 1;
    },
    b: 2
  };
  assertEquals([ [ "a", 1 ] ], Object.entries(aRemovesB));

  var aAddsB = { get a() { this.b = 2; return 1; } };
  assertEquals([ [ "a", 1 ] ], Object.entries(aAddsB));

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
  assertEquals([ [ "a", 1 ], [ "b", 2 ] ], Object.entries(aMakesBEnumerable));
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
    PACKED_SMI_ELEMENTS: [ [1, 2, 3], [ ["0", 1], ["1", 2], ["2", 3] ] ],
    HOLEY_SMI_ELEMENTS: [ [, , 3], [ ["2", 3] ] ],
    PACKED_ELEMENTS: [ [O1, O2, O3], [ ["0", O1], ["1", O2], ["2", O3] ] ],
    HOLEY_ELEMENTS: [ [, , O3], [ ["2", O3] ] ],
    PACKED_DOUBLE_ELEMENTS: [ [E, NaN, PI], [ ["0", E], ["1", NaN], ["2", PI] ] ],
    HOLEY_DOUBLE_ELEMENTS: [ [, , NaN], [ ["2", NaN] ] ],

    DICTIONARY_ELEMENTS: [ Object.defineProperties({ 10000: "world" }, {
       100: { enumerable: true, value: "hello", configurable: true},
        99: { enumerable: false, value: "nope", configurable: true}
      }), [ ["100", "hello"], ["10000", "world" ] ] ],
    FAST_SLOPPY_ARGUMENTS_ELEMENTS: [
        fastSloppyArguments("a", "b", "c"),
        [ ["0", "a"], ["1", "b"], ["2", "c"] ] ],
    SLOW_SLOPPY_ARGUMENTS_ELEMENTS: [
        slowSloppyArguments("a", "b", "c"),
        [ ["0", "a"], ["1", "b"], ["2", "c"], ["10000", "X"] ] ],

    FAST_STRING_WRAPPER_ELEMENTS: [ new String("str"),
        [ ["0", "s"], ["1", "t"], ["2", "r"]] ],
    SLOW_STRING_WRAPPER_ELEMENTS: [
        Object.defineProperties(new String("str"), {
          10000: { enumerable: false, value: "X", configurable: true},
          9999: { enumerable: true, value: "Y", configurable: true}
        }), [["0", "s"], ["1", "t"], ["2", "r"], ["9999", "Y"]] ],
  };

  for (let [kind, [object, expected]] of Object.entries(element_kinds)) {
    let result1 = Object.entries(object);
    %HeapObjectVerify(object);
    %HeapObjectVerify(result1);
    assertEquals(expected, result1, `fast Object.entries() with ${kind}`);

    let proxy = new Proxy(object, {});
    let result2 = Object.entries(proxy);
    %HeapObjectVerify(result2);
    assertEquals(result1, result2, `slow Object.entries() with ${kind}`);
  }

  function makeFastElements(array) {
    // Remove all possible getters.
    for (let k of Object.getOwnPropertyNames(this)) {
      if (k == "length") continue;
      delete this[k];
    }
    // Make the array large enough to trigger re-checking for compaction.
    this[1000] = 1;
    // Make the elements fast again.
    Array.prototype.unshift.call(this, 1.1);
  }

  // Test that changing the elements kind is supported.
  for (let [kind, [object, expected]] of Object.entries(element_kinds)) {
    if (kind == "FAST_STRING_WRAPPER_ELEMENTS") break;
    object.__defineGetter__(1, makeFastElements);
    let result1 = Object.entries(object).toString();
    %HeapObjectVerify(object);
    %HeapObjectVerify(result1);
  }

})();
