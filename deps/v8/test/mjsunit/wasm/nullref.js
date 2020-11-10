// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-anyref --experimental-wasm-eh

load('test/mjsunit/wasm/wasm-module-builder.js');

let kSig_n_n = makeSig([kWasmNullRef], [kWasmNullRef]);
let kSig_r_n = makeSig([kWasmNullRef], [kWasmAnyRef]);
let kSig_a_n = makeSig([kWasmNullRef], [kWasmAnyFunc]);
let kSig_e_n = makeSig([kWasmNullRef], [kWasmExnRef]);
let kSig_n_v = makeSig([], [kWasmNullRef]);
let kSig_n_r = makeSig([kWasmAnyRef], [kWasmNullRef]);
let kSig_n_a = makeSig([kWasmAnyFunc], [kWasmNullRef]);
let kSig_n_e = makeSig([kWasmExnRef], [kWasmNullRef]);
let kSig_v_n = makeSig([kWasmNullRef], []);
let kSig_v_in = makeSig([kWasmI32, kWasmNullRef], []);
let kSig_n_i = makeSig([kWasmI32], [kWasmNullRef]);
let kSig_r_i = makeSig([kWasmI32], [kWasmAnyRef]);
let kSig_e_i = makeSig([kWasmI32], [kWasmExnRef]);
let kSig_n_nni = makeSig([kWasmNullRef, kWasmNullRef, kWasmI32], [kWasmNullRef]);
let kSig_r_nni = makeSig([kWasmNullRef, kWasmNullRef, kWasmI32], [kWasmAnyRef]);
let kSig_a_nni = makeSig([kWasmNullRef, kWasmNullRef, kWasmI32], [kWasmAnyFunc]);
let kSig_e_nni = makeSig([kWasmNullRef, kWasmNullRef, kWasmI32], [kWasmExnRef]);

(function testNullRefIdentityFunction() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addFunction('nullRef', kSig_n_n)
      .addBody([kExprLocalGet, 0])
      .exportFunc();
  builder.addFunction('anyRef', kSig_r_n)
      .addBody([kExprLocalGet, 0])
      .exportFunc();
  builder.addFunction('funcRef', kSig_a_n)
      .addBody([kExprLocalGet, 0])
      .exportFunc();
  builder.addFunction('exnRef', kSig_e_n)
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  const instance = builder.instantiate();

  assertThrows(() => instance.exports.nullRef(a => a), TypeError);
  assertThrows(() => instance.exports.nullRef(print), TypeError);
  assertThrows(() => instance.exports.nullRef({'hello': 'world'}), TypeError);
  assertEquals(null, instance.exports.nullRef(null));
  assertEquals(null, instance.exports.anyRef(null));
  assertEquals(null, instance.exports.funcRef(null));
  assertEquals(null, instance.exports.exnRef(null));
})();

(function testNullRefFromAnyRefFail() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_n_r);
  builder.addFunction('main', sig_index)
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
})();

(function testNullRefFromFuncRefFail() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_n_a);
  builder.addFunction('main', sig_index)
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
})();

(function testNullRefFromExnRefFail() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_n_e);
  builder.addFunction('main', sig_index)
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
})();

(function testNullRefDefaultValue() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_n_v);
  builder.addFunction('main', sig_index)
      .addLocals({nullref_count: 1})
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main());
})();

(function testNullRefFromAnyRefFail() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_n_v);
  builder.addFunction('main', sig_index)
      .addLocals({anyref_count: 1})
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
})();

(function testNullRefFromFuncRefFail() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_n_v);
  builder.addFunction('main', sig_index)
      .addLocals({anyfunc_count: 1})
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
})();

(function testNullRefFromExnRefFail() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_n_v);
  builder.addFunction('main', sig_index)
      .addLocals({exnref_count: 1})
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
})();

(function testAssignNullRefToNullRefLocal() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_n_n);
  builder.addFunction('main', sig_index)
      .addBody([kExprRefNull, kExprLocalSet, 0, kExprLocalGet, 0])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main(null));
})();

(function testImplicitReturnNullRefAsNullRef() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_n_v);
  builder.addFunction('main', sig_index)
      .addBody([kExprRefNull])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main());
})();

(function testExplicitReturnNullRefAsNullRef() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_n_v);
  builder.addFunction('main', sig_index)
      .addBody([kExprRefNull, kExprReturn])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main());
})();

(function testGetNullRefGlobal() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const initialized = builder.addGlobal(kWasmNullRef, true)
      .exportAs('initialized');
  initialized.init = null;
  const uninitialized = builder.addGlobal(kWasmNullRef, true)
      .exportAs('uninitialized');
  const sig_n_v = builder.addType(kSig_n_v);
  const sig_v_n = builder.addType(kSig_v_n);
  const sig_v_v = builder.addType(kSig_v_v);
  builder.addFunction('get_initialized', sig_n_v)
      .addBody([kExprGlobalGet, initialized.index])
      .exportFunc();
  builder.addFunction('get_uninitialized', sig_n_v)
      .addBody([kExprGlobalGet, initialized.index])
      .exportFunc();
  builder.addFunction('set_initialized', sig_v_n)
      .addBody([kExprLocalGet, 0, kExprGlobalSet, initialized.index])
      .exportFunc();
  builder.addFunction('reset_initialized', sig_v_v)
      .addBody([kExprRefNull, kExprGlobalSet, initialized.index])
      .exportFunc();

  const instance = builder.instantiate();
  assertTrue(instance.exports.initialized instanceof WebAssembly.Global);
  assertTrue(instance.exports.uninitialized instanceof WebAssembly.Global);
  assertEquals(instance.exports.initialized.value, null);
  assertEquals(instance.exports.uninitialized.value, null);
  assertEquals(null, instance.exports.get_initialized());
  assertEquals(null, instance.exports.get_uninitialized());

  instance.exports.set_initialized(null);
  assertEquals(instance.exports.initialized.value, null);
  assertEquals(null, instance.exports.get_initialized());

  instance.exports.reset_initialized();
  assertEquals(instance.exports.initialized.value, null);
  assertEquals(null, instance.exports.get_initialized());
})();

(function testGetNullRefImportedGlobal() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const global_index = builder.addImportedGlobal("foo", "bar",
      kWasmNullRef);
  const sig_n_v = builder.addType(kSig_n_v);
  const sig_v_n = builder.addType(kSig_v_n);
  const sig_v_v = builder.addType(kSig_v_v);
  builder.addFunction('get', sig_n_v)
      .addBody([kExprGlobalGet, global_index])
      .exportFunc();

  assertThrows(() => builder.instantiate(), TypeError);
  assertThrows(() => builder.instantiate({foo: {}}), WebAssembly.LinkError);
  assertThrows(() => builder.instantiate({foo: {bar: {}}}),
      WebAssembly.LinkError);
  assertThrows(() => builder.instantiate({foo: {bar: a => a}}),
      WebAssembly.LinkError);

  const instance = builder.instantiate({foo: {bar: null}});
  assertEquals(null, instance.exports.get());
})();

(function testNullRefTable() {
  print(arguments.callee.name);
  let table = new WebAssembly.Table({element: "nullref", initial: 2});

  assertEquals(null, table.get(0));
  table.set(1, null);
  assertEquals(null, table.get(1));
  assertThrows(() => table.set(2, null), RangeError);

  table.grow(2);

  assertEquals(null, table.get(2));
  table.set(3, null);
  assertEquals(null, table.get(3));
  assertThrows(() => table.set(4, null), RangeError);

  assertThrows(() => table.set(0, {}), TypeError);
  assertThrows(() => table.set(0, a => a), TypeError);
})();

(function testAddNullRefTable() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const table = builder.addTable(kWasmNullRef, 3, 10);
  builder.addFunction('set_null', kSig_v_i)
      .addBody([kExprLocalGet, 0, kExprRefNull, kExprTableSet, table.index])
      .exportFunc();
  builder.addFunction('set', kSig_v_in)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprTableSet, table.index])
      .exportFunc();
  builder.addFunction('get_null', kSig_n_i)
      .addBody([kExprLocalGet, 0, kExprTableGet, table.index])
      .exportFunc();
  builder.addFunction('get_any', kSig_r_i)
      .addBody([kExprLocalGet, 0, kExprTableGet, table.index])
      .exportFunc();
  builder.addFunction('get_func', kSig_a_i)
      .addBody([kExprLocalGet, 0, kExprTableGet, table.index])
      .exportFunc();
  builder.addFunction('get_exn', kSig_e_i)
      .addBody([kExprLocalGet, 0, kExprTableGet, table.index])
      .exportFunc();

  const instance = builder.instantiate();
  instance.exports.set_null(1);
  instance.exports.set(2, null);

  assertEquals(null, instance.exports.get_null(0));
  assertEquals(null, instance.exports.get_null(1));
  assertEquals(null, instance.exports.get_null(2));
  assertEquals(null, instance.exports.get_any(0));
  assertEquals(null, instance.exports.get_any(1));
  assertEquals(null, instance.exports.get_any(2));
  assertEquals(null, instance.exports.get_func(0));
  assertEquals(null, instance.exports.get_func(1));
  assertEquals(null, instance.exports.get_func(2));
  assertEquals(null, instance.exports.get_exn(0));
  assertEquals(null, instance.exports.get_exn(1));
  assertEquals(null, instance.exports.get_exn(2));
})();

(function testImportNullRefTable() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const table_index = builder.addImportedTable("imp", "table", 2, 10,
      kWasmNullRef);
  builder.addFunction('get_null', kSig_n_i)
      .addBody([kExprLocalGet, 0, kExprTableGet, table_index])
      .exportFunc();
  builder.addFunction('get_any', kSig_r_i)
      .addBody([kExprLocalGet, 0, kExprTableGet, table_index])
      .exportFunc();
  builder.addFunction('get_func', kSig_a_i)
      .addBody([kExprLocalGet, 0, kExprTableGet, table_index])
      .exportFunc();
  builder.addFunction('get_exn', kSig_e_i)
      .addBody([kExprLocalGet, 0, kExprTableGet, table_index])
      .exportFunc();

  let table_func = new WebAssembly.Table({element: "anyfunc", initial: 2,
      maximum: 10});
  assertThrows(() => builder.instantiate({imp: {table: table_func}}),
      WebAssembly.LinkError, /imported table does not match the expected type/);

  let table_any = new WebAssembly.Table({element: "anyref", initial: 2,
      maximum: 10});
  assertThrows(() => builder.instantiate({imp: {table: table_any}}),
      WebAssembly.LinkError, /imported table does not match the expected type/);

  let table_null = new WebAssembly.Table({element: "nullref", initial: 2,
      maximum: 10});
  table_null.set(1, null);
  const instance = builder.instantiate({imp: {table: table_null}});

  assertEquals(null, instance.exports.get_null(0));
  assertEquals(null, instance.exports.get_null(1));
  assertEquals(null, instance.exports.get_any(0));
  assertEquals(null, instance.exports.get_any(1));
  assertEquals(null, instance.exports.get_func(0));
  assertEquals(null, instance.exports.get_func(1));
  assertEquals(null, instance.exports.get_exn(0));
  assertEquals(null, instance.exports.get_exn(1));

  assertThrows(() => instance.exports.get_null(2), WebAssembly.RuntimeError);
  table_null.grow(1);
  assertEquals(null, instance.exports.get_null(2));
})();

(function testImportNullRefTableIntoAnyRefTable() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const table_index = builder.addImportedTable("imp", "table", 2, 10,
      kWasmAnyRef);
  builder.addFunction('get', kSig_r_v)
      .addBody([kExprI32Const, 0, kExprTableGet, table_index])
      .exportFunc();

  let table_null = new WebAssembly.Table({element: "nullref", initial: 2,
      maximum: 10});
  assertThrows(() => builder.instantiate({imp: {table: table_null}}),
      WebAssembly.LinkError, /imported table does not match the expected type/);
})();

(function testSelectNullRef() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addFunction('select_null', kSig_n_nni)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
          kExprSelectWithType, 1, kWasmNullRef])
      .exportFunc();
  builder.addFunction('select_any', kSig_r_nni)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
          kExprSelectWithType, 1, kWasmAnyRef])
      .exportFunc();
  builder.addFunction('select_func', kSig_a_nni)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
          kExprSelectWithType, 1, kWasmAnyFunc])
      .exportFunc();
  builder.addFunction('select_exn', kSig_e_nni)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
          kExprSelectWithType, 1, kWasmExnRef])
      .exportFunc();
  builder.addFunction('select_null_as_any', kSig_r_nni)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
          kExprSelectWithType, 1, kWasmNullRef])
      .exportFunc();
  builder.addFunction('select_null_as_func', kSig_a_nni)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
          kExprSelectWithType, 1, kWasmNullRef])
      .exportFunc();
  builder.addFunction('select_null_as_exn', kSig_e_nni)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
          kExprSelectWithType, 1, kWasmNullRef])
      .exportFunc();

  const instance = builder.instantiate();

  assertEquals(null, instance.exports.select_null(null, null, 0));
  assertEquals(null, instance.exports.select_any(null, null, 0));
  assertEquals(null, instance.exports.select_func(null, null, 0));
  assertEquals(null, instance.exports.select_exn(null, null, 0));
  assertEquals(null, instance.exports.select_null_as_any(null, null, 0));
  assertEquals(null, instance.exports.select_null_as_func(null, null, 0));
  assertEquals(null, instance.exports.select_null_as_exn(null, null, 0));
})();
