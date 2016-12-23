// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

// Basic tests.

var outOfUint32RangeValue = 1e12;
var int32ButOob = 1073741824;

function assertTableIsValid(table) {
  assertSame(WebAssembly.Table.prototype, table.__proto__);
  assertSame(WebAssembly.Table, table.constructor);
  assertTrue(table instanceof Object);
  assertTrue(table instanceof WebAssembly.Table);
}

(function TestConstructor() {
  assertTrue(WebAssembly.Table instanceof Function);
  assertSame(WebAssembly.Table, WebAssembly.Table.prototype.constructor);
  assertTrue(WebAssembly.Table.prototype.grow instanceof Function);
  assertTrue(WebAssembly.Table.prototype.get instanceof Function);
  assertTrue(WebAssembly.Table.prototype.set instanceof Function);
  let desc = Object.getOwnPropertyDescriptor(WebAssembly.Table.prototype, 'length');
  assertTrue(desc.get instanceof Function);
  assertSame(undefined, desc.set);

  assertThrows(() => new WebAssembly.Table(), TypeError);
  assertThrows(() => new WebAssembly.Table(1), TypeError);
  assertThrows(() => new WebAssembly.Table(""), TypeError);

  assertThrows(() => new WebAssembly.Table({}), TypeError);
  assertThrows(() => new WebAssembly.Table({initial: 10}), TypeError);

  assertThrows(() => new WebAssembly.Table({element: 0, initial: 10}), TypeError);
  assertThrows(() => new WebAssembly.Table({element: "any", initial: 10}), TypeError);

  assertThrows(() => new WebAssembly.Table({element: "anyfunc", initial: -1}), RangeError);
  assertThrows(() => new WebAssembly.Table({element: "anyfunc", initial: outOfUint32RangeValue}), RangeError);

  assertThrows(() => new WebAssembly.Table({element: "anyfunc", initial: 10, maximum: -1}), RangeError);
  assertThrows(() => new WebAssembly.Table({element: "anyfunc", initial: 10, maximum: outOfUint32RangeValue}), RangeError);
  assertThrows(() => new WebAssembly.Table({element: "anyfunc", initial: 10, maximum: 9}), RangeError);

  assertThrows(() => new WebAssembly.Table({element: "anyfunc", initial: 0, maximum: int32ButOob}));

  let table = new WebAssembly.Table({element: "anyfunc", initial: 1});
  assertTableIsValid(table);
})();

(function TestConstructorWithMaximum() {
  let table = new WebAssembly.Table({element: "anyfunc", maximum: 10});
  assertTableIsValid(table);
})();

(function TestInitialIsUndefined() {
  // New memory with initial = undefined, which means initial = 0.
  let table = new WebAssembly.Table({element: "anyfunc", initial: undefined});
  assertTableIsValid(table);
})();

(function TestMaximumIsUndefined() {
  // New memory with maximum = undefined, which means maximum = 0.
  let table = new WebAssembly.Table({element: "anyfunc", initial: 0, maximum: undefined});
  assertTableIsValid(table);
})();

(function TestMaximumIsReadOnce() {
  var a = true;
  var desc = {element: "anyfunc", initial: 10};
  Object.defineProperty(desc, 'maximum', {get: function() {
    if (a) {
      a = false;
      return 16;
    }
    else {
      // Change the return value on the second call so it throws.
      return -1;
    }
  }});
  let table = new WebAssembly.Table(desc);
  assertTableIsValid(table);
})();

(function TestMaximumDoesHasProperty() {
  var hasPropertyWasCalled = false;
  var desc = {element: "anyfunc", initial: 10};
  var proxy = new Proxy({maximum: 16}, {
    has: function(target, name) { hasPropertyWasCalled = true; }
  });
  Object.setPrototypeOf(desc, proxy);
  let table = new WebAssembly.Table(desc);
  assertTableIsValid(table);
})();
