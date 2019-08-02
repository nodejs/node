// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-type-reflection

load('test/mjsunit/wasm/wasm-module-builder.js');

(function TestInvalidArgumentToType() {
  ["abc", 123, {}, _ => 0].forEach(function(invalidInput) {
    assertThrows(
      () => WebAssembly.Memory.type(invalidInput), TypeError,
        "WebAssembly.Memory.type(): Argument 0 must be a WebAssembly.Memory");
    assertThrows(
      () => WebAssembly.Table.type(invalidInput), TypeError,
        "WebAssembly.Table.type(): Argument 0 must be a WebAssembly.Table");
    assertThrows(
      () => WebAssembly.Global.type(invalidInput), TypeError,
        "WebAssembly.Global.type(): Argument 0 must be a WebAssembly.Global");
    assertThrows(
      () => WebAssembly.Function.type(invalidInput), TypeError,
        "WebAssembly.Function.type(): Argument 0 must be a WebAssembly.Function");
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

  assertThrows(
    () => WebAssembly.Function.type(
      new WebAssembly.Memory({initial:1})), TypeError,
      "WebAssembly.Function.type(): Argument 0 must be a WebAssembly.Function");
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

(function TestFunctionConstructor() {
  let toolong = new Array(1000 + 1);
  let desc = Object.getOwnPropertyDescriptor(WebAssembly, 'Function');
  assertEquals(typeof desc.value, 'function');
  assertTrue(desc.writable);
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  // TODO(7742): The length should probably be 2 instead.
  assertEquals(WebAssembly.Function.length, 1);
  assertEquals(WebAssembly.Function.name, 'Function');
  assertThrows(
      () => WebAssembly.Function(), TypeError, /must be invoked with 'new'/);
  assertThrows(
    () => new WebAssembly.Function(), TypeError,
    /Argument 0 must be a function type/);
  assertThrows(
    () => new WebAssembly.Function({}), TypeError,
    /Argument 0 must be a function type with 'parameters'/);
  assertThrows(
    () => new WebAssembly.Function({parameters:[]}), TypeError,
    /Argument 0 must be a function type with 'results'/);
  assertThrows(
    () => new WebAssembly.Function({parameters:['foo'], results:[]}), TypeError,
    /Argument 0 parameter type at index #0 must be a value type/);
  assertThrows(
    () => new WebAssembly.Function({parameters:[], results:['foo']}), TypeError,
    /Argument 0 result type at index #0 must be a value type/);
  assertThrows(
    () => new WebAssembly.Function({parameters:toolong, results:[]}), TypeError,
    /Argument 0 contains too many parameters/);
  assertThrows(
    () => new WebAssembly.Function({parameters:[], results:toolong}), TypeError,
    /Argument 0 contains too many results/);
  assertThrows(
    () => new WebAssembly.Function({parameters:[], results:[]}), TypeError,
    /Argument 1 must be a function/);
  assertThrows(
    () => new WebAssembly.Function({parameters:[], results:[]}, {}), TypeError,
    /Argument 1 must be a function/);
  assertDoesNotThrow(
    () => new WebAssembly.Function({parameters:[], results:[]}, _ => 0));
})();

(function TestFunctionConstructedFunction() {
  let fun = new WebAssembly.Function({parameters:[], results:[]}, _ => 0);
  assertTrue(fun instanceof WebAssembly.Function);
  assertTrue(fun instanceof Function);
  assertTrue(fun instanceof Object);
  assertSame(fun.__proto__, WebAssembly.Function.prototype);
  assertSame(fun.__proto__.__proto__, Function.prototype);
  assertSame(fun.__proto__.__proto__.__proto__, Object.prototype);
  assertSame(fun.constructor, WebAssembly.Function);
  assertEquals(typeof fun, 'function');
  // TODO(7742): Enable once it is callable.
  // assertDoesNotThrow(() => fun());
})();

(function TestFunctionExportedFunction() {
  let builder = new WasmModuleBuilder();
  builder.addFunction("fun", kSig_v_v).addBody([]).exportFunc();
  let instance = builder.instantiate();
  let fun = instance.exports.fun;
  assertTrue(fun instanceof WebAssembly.Function);
  assertTrue(fun instanceof Function);
  assertTrue(fun instanceof Object);
  assertSame(fun.__proto__, WebAssembly.Function.prototype);
  assertSame(fun.__proto__.__proto__, Function.prototype);
  assertSame(fun.__proto__.__proto__.__proto__, Object.prototype);
  assertSame(fun.constructor, WebAssembly.Function);
  assertEquals(typeof fun, 'function');
  assertDoesNotThrow(() => fun());
})();

(function TestFunctionTypeOfExportedFunction() {
  let testcases = [
    [kSig_v_v, {parameters:[], results:[]}],
    [kSig_v_i, {parameters:["i32"], results:[]}],
    [kSig_i_l, {parameters:["i64"], results:["i32"]}],
    [kSig_v_ddi, {parameters:["f64", "f64", "i32"], results:[]}],
    [kSig_f_f, {parameters:["f32"], results:["f32"]}],
  ];
  testcases.forEach(function([sig, expected]) {
    let builder = new WasmModuleBuilder();
    builder.addFunction("fun", sig).addBody([kExprUnreachable]).exportFunc();
    let instance = builder.instantiate();
    let type = WebAssembly.Function.type(instance.exports.fun);
    assertEquals(expected, type)
  });
})();
