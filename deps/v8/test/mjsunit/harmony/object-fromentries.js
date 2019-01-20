// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-object-from-entries

const fromEntries = Object.fromEntries;
const ObjectPrototype = Object.prototype;
const ObjectPrototypeHasOwnProperty = ObjectPrototype.hasOwnProperty;
function hasOwnProperty(O, Name) {
  if (O === undefined || O === null) return false;
  return ObjectPrototypeHasOwnProperty.call(O, Name);
}

let test = {
  methodExists() {
    assertTrue(hasOwnProperty(Object, "fromEntries"));
    assertEquals("function", typeof Object.fromEntries);
  },

  methodLength() {
    assertEquals(1, Object.fromEntries.length);
  },

  methodName() {
    assertEquals("fromEntries", Object.fromEntries.name);
  },

  methodPropertyDescriptor() {
    let descriptor = Object.getOwnPropertyDescriptor(Object, "fromEntries");
    assertFalse(descriptor.enumerable);
    assertTrue(descriptor.configurable);
    assertTrue(descriptor.writable);
    assertEquals(descriptor.value, Object.fromEntries);
  },

  exceptionIfNotCoercible() {
    assertThrows(() => fromEntries(null), TypeError);
    assertThrows(() => fromEntries(undefined), TypeError);
  },

  exceptionIfNotIterable() {
    let nonIterable = [1, 2, 3, 4, 5];
    Object.defineProperty(nonIterable, Symbol.iterator, { value: undefined });
    assertThrows(() => fromEntries(nonIterable), TypeError);
  },

  exceptionIfGetIteratorThrows() {
    let iterable = [1, 2, 3, 4, 5];
    class ThrewDuringGet {};
    Object.defineProperty(iterable, Symbol.iterator, {
      get() { throw new ThrewDuringGet(); }
    });
    assertThrows(() => fromEntries(iterable), ThrewDuringGet);
  },

  exceptionIfCallIteratorThrows() {
    let iterable = [1, 2, 3, 4, 5];
    class ThrewDuringCall {};
    iterable[Symbol.iterator] = function() {
      throw new ThrewDuringCall();
    }
    assertThrows(() => fromEntries(iterable), ThrewDuringCall);
  },

  exceptionIfIteratorNextThrows() {
    let iterable = [1, 2, 3, 4, 5];
    class ThrewDuringIteratorNext {}
    iterable[Symbol.iterator] = function() {
      return {
        next() { throw new ThrewDuringIteratorNext; },
        return() {
          throw new Error(
              "IteratorClose must not be performed if IteratorStep throws");
        },
      }
    }
    assertThrows(() => fromEntries(iterable), ThrewDuringIteratorNext);
  },

  exceptionIfIteratorCompleteThrows() {
    let iterable = [1, 2, 3, 4, 5];
    class ThrewDuringIteratorComplete {}
    iterable[Symbol.iterator] = function() {
      return {
        next() {
          return {
            get value() { throw new Error(
                "IteratorValue must not be performed before IteratorComplete");
            },
            get done() {
              throw new ThrewDuringIteratorComplete();
            }
          }
          throw new ThrewDuringIteratorNext;
        },
        return() {
          throw new Error(
              "IteratorClose must not be performed if IteratorStep throws");
        },
      }
    }
    assertThrows(() => fromEntries(iterable), ThrewDuringIteratorComplete);
  },

  exceptionIfEntryIsNotObject() {
    {
      // Fast path (Objects/Smis)
      let iterables = [[null], [undefined], [1], [NaN], [false], [Symbol()],
                       [""]];
      for (let iterable of iterables) {
        assertThrows(() => fromEntries(iterable), TypeError);
      }
    }
    {
      // Fast path (Doubles)
      let iterable = [3.7, , , 3.6, 1.1, -0.4];
      assertThrows(() => fromEntries(iterable), TypeError);
    }
    {
      // Slow path
      let i = 0;
      let values = [null, undefined, 1, NaN, false, Symbol(), ""];
      let iterable = {
        [Symbol.iterator]() { return this; },
        next() {
          return {
            done: i >= values.length,
            value: values[i++],
          }
        },
      };
      for (let k = 0; k < values.length; ++k) {
        assertThrows(() => fromEntries(iterable), TypeError);
      }
      assertEquals({}, fromEntries(iterable));
    }
  },

  returnIfEntryIsNotObject() {
    // Only observable/verifiable in the slow path :(
    let i = 0;
    let didCallReturn = false;
    let values = [null, undefined, 1, NaN, false, Symbol(), ""];
    let iterable = {
      [Symbol.iterator]() { return this; },
      next() {
        return {
          done: i >= values.length,
          value: values[i++],
        }
      },
      return() { didCallReturn = true; throw new Error("Unused!"); }
    };
    for (let k = 0; k < values.length; ++k) {
      didCallReturn = false;
      assertThrows(() => fromEntries(iterable), TypeError);
      assertTrue(didCallReturn);
    }
    assertEquals({}, fromEntries(iterable));
  },

  returnIfEntryKeyAccessorThrows() {
    class ThrewDuringKeyAccessor {};
    let entries = [{ get 0() { throw new ThrewDuringKeyAccessor(); },
                     get 1() { throw new Error("Unreachable!"); } }];
    let didCallReturn = false;
    let iterator = entries[Symbol.iterator]();
    iterator.return = function() {
      didCallReturn = true;
      throw new Error("Unused!");
    }
    assertThrows(() => fromEntries(iterator), ThrewDuringKeyAccessor);
    assertTrue(didCallReturn);
  },

  returnIfEntryKeyAccessorThrows() {
    class ThrewDuringValueAccessor {};
    let entries = [{ get 1() { throw new ThrewDuringValueAccessor(); },
                         0: "key",
                      }];
    let didCallReturn = false;
    let iterator = entries[Symbol.iterator]();
    iterator.return = function() {
      didCallReturn = true;
      throw new Error("Unused!");
    };
    assertThrows(() => fromEntries(iterator), ThrewDuringValueAccessor);
    assertTrue(didCallReturn);
  },

  returnIfKeyToStringThrows() {
    class ThrewDuringKeyToString {};
    let operations = [];
    let entries = [{
      get 0() {
        operations.push("[[Get]] key");
        return {
          toString() {
            operations.push("toString(key)");
            throw new ThrewDuringKeyToString();
          },
          valueOf() {
            operations.push("valueOf(key)");
          }
        };
      },
      get 1() {
        operations.push("[[Get]] value");
        return "value";
      },
    }];

    let iterator = entries[Symbol.iterator]();
    iterator.return = function() {
      operations.push("IteratorClose");
      throw new Error("Unused!");
    };
    assertThrows(() => fromEntries(iterator), ThrewDuringKeyToString);
    assertEquals([
      "[[Get]] key",
      "[[Get]] value",
      "toString(key)",
      "IteratorClose",
    ], operations);
  },

  throwsIfIteratorValueThrows() {
    let iterable = [1, 2, 3, 4, 5];
    class ThrewDuringIteratorValue {}
    iterable[Symbol.iterator] = function() {
      return {
        next() {
          return {
            get value() { throw new ThrewDuringIteratorValue(); },
            get done() { return false; }
          }
          throw new ThrewDuringIteratorNext;
        },
        return() {
          throw new Error(
              "IteratorClose must not be performed if IteratorStep throws");
        },
      }
    }
    assertThrows(() => fromEntries(iterable), ThrewDuringIteratorValue);
  },

  emptyIterable() {
    let iterables = [[], new Set(), new Map()];
    for (let iterable of iterables) {
      let result = fromEntries(iterable);
      assertEquals({}, result);
      assertEquals(ObjectPrototype, result.__proto__);
    }
  },

  keyOrderFastPath() {
    let entries = [
      ["z", 1],
      ["y", 2],
      ["x", 3],
      ["y", 4],
      [100, 0],
    ];
    let result = fromEntries(entries);
    assertEquals({
      100: 0,
      z: 1,
      y: 4,
      x: 3,
    }, result);
    assertEquals(["100", "z", "y", "x"], Object.keys(result));
  },

  keyOrderSlowPath() {
    let entries = [
      ["z", 1],
      ["y", 2],
      ["x", 3],
      ["y", 4],
      [100, 0],
    ];
    let i = 0;
    let iterable = {
      [Symbol.iterator]() { return this; },
      next() {
        return {
          done: i >= entries.length,
          value: entries[i++]
        }
      },
      return() { throw new Error("Unreachable!"); }
    };
    let result = fromEntries(iterable);
    assertEquals({
      100: 0,
      z: 1,
      y: 4,
      x: 3,
    }, result);
    assertEquals(["100", "z", "y", "x"], Object.keys(result));
  },

  doesNotUseIteratorForKeyValuePairFastCase() {
    class Entry {
      constructor(k, v) {
        this[0] = k;
        this[1] = v;
      }
      get [Symbol.iterator]() {
        throw new Error("Should not load Symbol.iterator from Entry!");
      }
    }
    function e(k, v) { return new Entry(k, v); }
    let entries = [e(100, 0), e('z', 1), e('y', 2), e('x', 3), e('y', 4)];
    let result = fromEntries(entries);
    assertEquals({
      100: 0,
      z: 1,
      y: 4,
      x: 3,
    }, result);
  },

  doesNotUseIteratorForKeyValuePairSlowCase() {
    class Entry {
      constructor(k, v) {
        this[0] = k;
        this[1] = v;
      }
      get [Symbol.iterator]() {
        throw new Error("Should not load Symbol.iterator from Entry!");
      }
    }
    function e(k, v) { return new Entry(k, v); }
    let entries = new Set(
        [e(100, 0), e('z', 1), e('y', 2), e('x', 3), e('y', 4)]);
    let result = fromEntries(entries);
    assertEquals({
      100: 0,
      z: 1,
      y: 4,
      x: 3,
    }, result);
  },

  createDataPropertyFastCase() {
    Object.defineProperty(ObjectPrototype, "property", {
      configurable: true,
      get() { throw new Error("Should not invoke getter on prototype!"); },
      set() { throw new Error("Should not invoke setter on prototype!"); },
    });

    let entries = [["property", "value"]];
    let result = fromEntries(entries);
    assertEquals(result.property, "value");
    delete ObjectPrototype.property;
  },

  createDataPropertySlowCase() {
    Object.defineProperty(ObjectPrototype, "property", {
      configurable: true,
      get() { throw new Error("Should not invoke getter on prototype!"); },
      set() { throw new Error("Should not invoke setter on prototype!"); },
    });

    let entries = new Set([["property", "value"]]);
    let result = fromEntries(entries);
    assertEquals(result.property, "value");
    delete ObjectPrototype.property;
  },

  keyToPrimitiveMutatesArrayInFastCase() {
    let mySymbol = Symbol();
    let entries = [[0, 1], ["a", 2], [{
      [Symbol.toPrimitive]() {
        // The fast path should bail out if a key is a JSReceiver, otherwise
        // assumptions about the structure of the iterable can change. If the
        // fast path doesn't bail out, the 4th key would be "undefined".
        delete entries[3][0];
        entries[3].__proto__ = { 0: "shfifty", };
        return mySymbol;
      },
    }, 3], [3, 4]];
    let result = fromEntries(entries);
    assertEquals({
      0: 1,
      "a": 2,
      [mySymbol]: 3,
      "shfifty": 4,
    }, result);
    assertEquals(["0", "a", "shfifty", mySymbol], Reflect.ownKeys(result));
  },

  keyToStringMutatesArrayInFastCase() {
    let mySymbol = Symbol();
    let entries = [[mySymbol, 1], [0, 2], [{
      toString() {
        delete entries[3][0];
        entries[3].__proto__ = { 0: "shfifty", };
        return "z";
      },
      valueOf() { throw new Error("Unused!"); }
    }, 3], [3, 4]];
    let result = fromEntries(entries);
    assertEquals({
      [mySymbol]: 1,
      0: 2,
      "z": 3,
      "shfifty": 4,
    }, result);
    assertEquals(["0", "z", "shfifty", mySymbol], Reflect.ownKeys(result));
  },

  keyValueOfMutatesArrayInFastCase() {
    let mySymbol = Symbol();
    let entries = [[mySymbol, 1], ["z", 2], [{
      toString: undefined,
      valueOf() {
        delete entries[3][0];
        entries[3].__proto__ = { 0: "shfifty", };
        return 0;
      },
    }, 3], [3, 4]];
    let result = fromEntries(entries);
    assertEquals({
      [mySymbol]: 1,
      "z": 2,
      0: 3,
      "shfifty": 4,
    }, result);
    assertEquals(["0", "z", "shfifty", mySymbol], Reflect.ownKeys(result));
  },
}

for (let t of Reflect.ownKeys(test)) {
  test[t]();
}
