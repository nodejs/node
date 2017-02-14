// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

'use strict';

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

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

  let table;
  table = new WebAssembly.Table({element: "anyfunc", initial: 1});
  assertTableIsValid(table);
  assertEquals(1, table.length);
  assertEquals(null, table.get(0));

  table = new WebAssembly.Table({element: "anyfunc", initial: "2"});
  assertTableIsValid(table);
  assertEquals(2, table.length);
  assertEquals(null, table.get(0));
  assertEquals(null, table.get(1));

  table = new WebAssembly.Table({element: "anyfunc", initial: {valueOf() { return "1" }}});
  assertTableIsValid(table);
  assertEquals(1, table.length);
  assertEquals(null, table.get(0));

  table = new WebAssembly.Table({element: "anyfunc", initial: undefined});
  assertTableIsValid(table);
  assertEquals(0, table.length);

  table = new WebAssembly.Table({element: "anyfunc"});
  assertTableIsValid(table);
  assertEquals(0, table.length);

  table = new WebAssembly.Table({element: "anyfunc", maximum: 10});
  assertTableIsValid(table);
  assertEquals(0, table.length);

  table = new WebAssembly.Table({element: "anyfunc", maximum: "10"});
  assertTableIsValid(table);
  assertEquals(0, table.length);

  table = new WebAssembly.Table({element: "anyfunc", maximum: {valueOf() { return "10" }}});
  assertTableIsValid(table);
  assertEquals(0, table.length);

  table = new WebAssembly.Table({element: "anyfunc", initial: 0, maximum: undefined});
  assertTableIsValid(table);
  assertEquals(0, table.length);
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

(function TestLength() {
  for (let i = 0; i < 10; ++i) {
    let table = new WebAssembly.Table({element: "anyfunc", initial: i});
    assertEquals(i, table.length);
  }

  assertThrows(() => WebAssembly.Table.prototype.length.call([]), TypeError);
})();

(function TestGet() {
  let table = new WebAssembly.Table({element: "anyfunc", initial: 10});

  for (let i = 0; i < table.length; ++i) {
    assertEquals(null, table.get(i));
    assertEquals(null, table.get(String(i)));
  }
  for (let key of [0.4, "", NaN, {}, [], () => {}]) {
    assertEquals(null, table.get(key));
  }
  for (let key of [-1, table.length, table.length * 10]) {
    assertThrows(() => table.get(key), RangeError);
  }
  assertThrows(() => table.get(Symbol()), TypeError);
  assertThrows(() => WebAssembly.Table.prototype.get.call([], 0), TypeError);
})();

(function TestSet() {
  let builder = new WasmModuleBuilder;
  builder.addExport("wasm", builder.addFunction("", kSig_v_v));
  builder.addExport("host", builder.addImportWithModule("test", "f", kSig_v_v));
  let {wasm, host} = builder.instantiate({test: {f() {}}}).exports;

  let table = new WebAssembly.Table({element: "anyfunc", initial: 10});

  for (let f of [wasm, host]) {
    for (let i = 0; i < table.length; ++i) table.set(i, null);
    for (let i = 0; i < table.length; ++i) {
      assertSame(null, table.get(i));
      assertSame(undefined, table.set(i, f));
      assertSame(f, table.get(i));
    }

    for (let i = 0; i < table.length; ++i) table.set(i, null);
    for (let i = 0; i < table.length; ++i) {
      assertSame(null, table.get(i));
      assertSame(undefined, table.set(String(i), f));
      assertSame(f, table.get(i));
    }

    for (let key of [0.4, "", NaN, {}, [], () => {}]) {
      assertSame(undefined, table.set(0, null));
      assertSame(undefined, table.set(key, f));
      assertSame(f, table.get(0));
    }

    for (let key of [-1, table.length, table.length * 10]) {
      assertThrows(() => table.set(key, f), RangeError);
    }

    assertThrows(() => table.set(0), TypeError);
    for (let val of [undefined, 0, "", {}, [], () => {}]) {
      assertThrows(() => table.set(0, val), TypeError);
    }

    assertThrows(() => table.set(Symbol(), f), TypeError);
    assertThrows(() => WebAssembly.Table.prototype.set.call([], 0, f),
                 TypeError);
  }
})();

(function TestGrow() {
  let builder = new WasmModuleBuilder;
  builder.addExport("wasm", builder.addFunction("", kSig_v_v));
  builder.addExport("host", builder.addImportWithModule("test", "f", kSig_v_v));
  let {wasm, host} = builder.instantiate({test: {f() {}}}).exports;

  function init(table) {
    for (let i = 0; i < 5; ++i) table.set(i, wasm);
    for (let i = 15; i < 20; ++i) table.set(i, host);
  }
  function check(table) {
    for (let i = 0; i < 5; ++i) assertSame(wasm, table.get(i));
    for (let i = 6; i < 15; ++i) assertSame(null, table.get(i));
    for (let i = 15; i < 20; ++i) assertSame(host, table.get(i));
    for (let i = 21; i < table.length; ++i) assertSame(null, table.get(i));
  }

  let table = new WebAssembly.Table({element: "anyfunc", initial: 20});
  init(table);
  check(table);
  table.grow(0);
  check(table);
  table.grow(10);
  check(table);
  assertThrows(() => table.grow(-10), RangeError);

  table = new WebAssembly.Table({element: "anyfunc", initial: 20, maximum: 25});
  init(table);
  check(table);
  table.grow(0);
  check(table);
  table.grow(5);
  check(table);
  table.grow(0);
  check(table);
  assertThrows(() => table.grow(1), RangeError);
  assertThrows(() => table.grow(-10), RangeError);

  assertThrows(() => WebAssembly.Table.prototype.grow.call([], 0), TypeError);
})();
