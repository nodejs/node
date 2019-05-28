// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-type-reflection

load('test/mjsunit/wasm/wasm-module-builder.js');

(function TestInvalidArgumentToType() {
  ["abc", 123, {}].forEach(function(invalidInput) {
    assertThrows(
      () => WebAssembly.Memory.type(invalidInput), TypeError,
        "WebAssembly.Memory.type(): Argument 0 must be a WebAssembly.Memory");
    assertThrows(
      () => WebAssembly.Table.type(invalidInput), TypeError,
        "WebAssembly.Table.type(): Argument 0 must be a WebAssembly.Table");
    assertThrows(
      () => WebAssembly.Global.type(invalidInput), TypeError,
        "WebAssembly.Global.type(): Argument 0 must be a WebAssembly.Global");
  });

  assertThrows(
    () => WebAssembly.Memory.type(
      new WebAssembly.Table({initial:1, element: "anyfunc"})),
      TypeError,
      "WebAssembly.Memory.type(): Argument 0 must be a WebAssembly.Memory");

  assertThrows(
    () => WebAssembly.Table.type(
      new WebAssembly.Memory({initial:1})), TypeError,
      "WebAssembly.Table.type(): Argument 0 must be a WebAssembly.Table");

  assertThrows(
    () => WebAssembly.Global.type(
      new WebAssembly.Memory({initial:1})), TypeError,
      "WebAssembly.Global.type(): Argument 0 must be a WebAssembly.Global");
})();

(function TestMemoryType() {
  let mem = new WebAssembly.Memory({initial: 1});
  let type = WebAssembly.Memory.type(mem);
  assertEquals(1, type.minimum);
  assertEquals(1, Object.getOwnPropertyNames(type).length);

  mem = new WebAssembly.Memory({initial: 2, maximum: 15});
  type = WebAssembly.Memory.type(mem);
  assertEquals(2, type.minimum);
  assertEquals(15, type.maximum);
  assertEquals(2, Object.getOwnPropertyNames(type).length);
})();

(function TestTableType() {
  let table = new WebAssembly.Table({initial: 1, element: "anyfunc"});
  let type = WebAssembly.Table.type(table);
  assertEquals(1, type.minimum);
  assertEquals("anyfunc", type.element);
  assertEquals(undefined, type.maximum);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  table = new WebAssembly.Table({initial: 2, maximum: 15, element: "anyfunc"});
  type = WebAssembly.Table.type(table);
  assertEquals(2, type.minimum);
  assertEquals(15, type.maximum);
  assertEquals("anyfunc", type.element);
  assertEquals(3, Object.getOwnPropertyNames(type).length);
})();

(function TestGlobalType() {
  let global = new WebAssembly.Global({value: "i32", mutable: true});
  let type = WebAssembly.Global.type(global);
  assertEquals("i32", type.value);
  assertEquals(true, type.mutable);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  global = new WebAssembly.Global({value: "i32"});
  type = WebAssembly.Global.type(global);
  assertEquals("i32", type.value);
  assertEquals(false, type.mutable);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  global = new WebAssembly.Global({value: "i64"});
  type = WebAssembly.Global.type(global);
  assertEquals("i64", type.value);
  assertEquals(false, type.mutable);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  global = new WebAssembly.Global({value: "f32"});
  type = WebAssembly.Global.type(global);
  assertEquals("f32", type.value);
  assertEquals(false, type.mutable);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  global = new WebAssembly.Global({value: "f64"});
  type = WebAssembly.Global.type(global);
  assertEquals("f64", type.value);
  assertEquals(false, type.mutable);
  assertEquals(2, Object.getOwnPropertyNames(type).length);
})();

(function TestMemoryConstructorWithMinimum() {
  let mem = new WebAssembly.Memory({minimum: 1});
  assertTrue(mem instanceof WebAssembly.Memory);
  let type = WebAssembly.Memory.type(mem);
  assertEquals(1, type.minimum);
  assertEquals(1, Object.getOwnPropertyNames(type).length);

  mem = new WebAssembly.Memory({minimum: 1, maximum: 5});
  assertTrue(mem instanceof WebAssembly.Memory);
  type = WebAssembly.Memory.type(mem);
  assertEquals(1, type.minimum);
  assertEquals(5, type.maximum);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  mem = new WebAssembly.Memory({minimum: 1, initial: 2});
  assertTrue(mem instanceof WebAssembly.Memory);
  type = WebAssembly.Memory.type(mem);
  assertEquals(2, type.minimum);
  assertEquals(1, Object.getOwnPropertyNames(type).length);

  mem = new WebAssembly.Memory({minimum: 1, initial: 2, maximum: 5});
  assertTrue(mem instanceof WebAssembly.Memory);
  type = WebAssembly.Memory.type(mem);
  assertEquals(2, type.minimum);
  assertEquals(5, type.maximum);
  assertEquals(2, Object.getOwnPropertyNames(type).length);
})();

(function TestTableConstructorWithMinimum() {
  let table = new WebAssembly.Table({minimum: 1, element: 'anyfunc'});
  assertTrue(table instanceof WebAssembly.Table);
  let type = WebAssembly.Table.type(table);
  assertEquals(1, type.minimum);
  assertEquals('anyfunc', type.element);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  table = new WebAssembly.Table({minimum: 1, element: 'anyfunc', maximum: 5});
  assertTrue(table instanceof WebAssembly.Table);
  type = WebAssembly.Table.type(table);
  assertEquals(1, type.minimum);
  assertEquals(5, type.maximum);
  assertEquals('anyfunc', type.element);
  assertEquals(3, Object.getOwnPropertyNames(type).length);

  table = new WebAssembly.Table({minimum: 1, initial: 2, element: 'anyfunc'});
  assertTrue(table instanceof WebAssembly.Table);
  type = WebAssembly.Table.type(table);
  assertEquals(2, type.minimum);
  assertEquals('anyfunc', type.element);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  table = new WebAssembly.Table({minimum: 1, initial: 2, element: 'anyfunc',
                                 maximum: 5});
  assertTrue(table instanceof WebAssembly.Table);
  type = WebAssembly.Table.type(table);
  assertEquals(2, type.minimum);
  assertEquals(5, type.maximum);
  assertEquals('anyfunc', type.element);
  assertEquals(3, Object.getOwnPropertyNames(type).length);
})();
