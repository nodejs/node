// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

let kMaxTableSize = 10000000;

function addFunctions(builder) {
  let sig_index = builder.addType(kSig_i_ii);
  let mul = builder.addFunction("mul", sig_index)
    .addBody([
      kExprGetLocal, 0,  // --
      kExprGetLocal, 1,  // --
      kExprI32Mul        // --
    ]);
  let add = builder.addFunction("add", sig_index)
    .addBody([
      kExprGetLocal, 0,  // --
      kExprGetLocal, 1,  // --
      kExprI32Add        // --
    ]);
  let sub = builder.addFunction("sub", sig_index)
    .addBody([
      kExprGetLocal, 0,  // --
      kExprGetLocal, 1,  // --
      kExprI32Sub        // --
    ]);
  return {mul: mul, add: add, sub: sub};
}

function testBounds(func, table) {
  for (let i = 0; i < table.length; i++) {
    assertEquals(0, func(i));
  }
  let l = table.length;
  let oob = [l, l + 1, l + 2, l * 2, l * 3, l + 10000];
  for (let i of oob) {
    assertThrows(() => func(i));
  }
}

function addMain(builder) {
  builder.addImportedTable("imp", "table", 0, kMaxTableSize);
  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprI32Const, 0,
      kExprGetLocal, 0,
      kExprCallIndirect, 0, kTableZero])
    .exportAs("main");
}

let id = (() => {  // identity exported function
  let builder = new WasmModuleBuilder();
  builder.addFunction("id", kSig_i_i)
    .addBody([kExprGetLocal, 0])
    .exportAs("id");
  let module = new WebAssembly.Module(builder.toBuffer());
  return (new WebAssembly.Instance(builder.toModule())).exports.id;
})();

(function TableGrowBoundsCheck() {
  print("TableGrowBoundsCheck");
  let builder = new WasmModuleBuilder();
  addMain(builder);
  let module = new WebAssembly.Module(builder.toBuffer());
  let table = new WebAssembly.Table({element: "anyfunc",
                                     initial: 1, maximum:kMaxTableSize});
  function fillTable() {
    for (let i = 0; i < table.length; i++) table.set(i, id);
    return table;
  }
  fillTable();
  let instance1 = new WebAssembly.Instance(module, {imp: {table:table}});
  testBounds(instance1.exports.main, table);

  for (let i = 0; i < 4; i++) {
    table.grow(1);
    fillTable(table);
    testBounds(instance1.exports.main, table);
  }
  let instance2 = new WebAssembly.Instance(module, {imp: {table:table}});
  testBounds(instance2.exports.main, table);

  for (let i = 0; i < 4; i++) {
    table.grow(1);
    fillTable(table);
    testBounds(instance1.exports.main, table);
    testBounds(instance2.exports.main, table);
  }
})();

(function TableGrowBoundsZeroInitial() {
  print("TableGrowBoundsZeroInitial");
  let builder = new WasmModuleBuilder();
  addMain(builder);
  let module = new WebAssembly.Module(builder.toBuffer());
  var table = new WebAssembly.Table({element: "anyfunc",
                                     initial: 0, maximum:kMaxTableSize});
  function growTableByOne() {
    table.grow(1);
    table.set(table.length - 1, id);
  }
  let instance1 = new WebAssembly.Instance(module, {imp: {table:table}});
  testBounds(instance1.exports.main, table);

  for (let i = 0; i < 4; i++) {
    growTableByOne();
    testBounds(instance1.exports.main, table);
  }
  let instance2 = new WebAssembly.Instance(module, {imp: {table:table}});
  testBounds(instance2.exports.main, table);

  for (let i = 0; i < 4; i++) {
    growTableByOne();
    testBounds(instance1.exports.main, table);
    testBounds(instance2.exports.main, table);
  }
})();

(function InstancesShareTableAndGrowTest() {
  print("InstancesShareTableAndGrowTest");
  let builder = new WasmModuleBuilder();
  let funcs = addFunctions(builder);
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprI32Const, 15,  // --
      kExprGetLocal, 0,   // --
      kExprGetLocal, 1,   // --
      kExprCallIndirect, 0, kTableZero])  // --
    .exportAs("main");

  builder.addImportedTable("q", "table", 5, 32);
  let g = builder.addImportedGlobal("q", "base", kWasmI32);
  builder.addFunctionTableInit(g, true,
      [funcs.mul.index, funcs.add.index, funcs.sub.index]);
  builder.addExportOfKind("table", kExternalTable, 0);
  let module = new WebAssembly.Module(builder.toBuffer());
  let t1 = new WebAssembly.Table({element: "anyfunc",
    initial: 5, maximum: 30});

  for (let i = 0; i < 5; i++) {
    print("base = " + i);
    let instance = new WebAssembly.Instance(module, {q: {base: i, table: t1}});
    main = instance.exports.main;
    assertEquals(i * 5 + 5, t1.length);

    // mul
    assertEquals(15, main(1, i));
    assertEquals(30, main(2, i));
    // add
    assertEquals(20, main(5, i+1));
    assertEquals(25, main(10, i+1));
    //sub
    assertEquals(10, main(5, i+2));
    assertEquals(5, main(10, i+2));

    assertThrows(() => t1.set(t1.length, id), RangeError);
    assertThrows(() => t1.set(t1.length + 5, id), RangeError);
    assertEquals(i * 5 + 5, t1.grow(5));
  }

  t1.set(t1.length - 1, id);
  assertThrows(() => t1.set(t1.length, id), RangeError);
  assertThrows(() => t1.grow(2), RangeError);
})();

(function ModulesShareTableAndGrowTest() {
  print("ModulesShareTableAndGrowTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let sig_i_i = builder.addType(kSig_i_i);
  let sig_i_v = builder.addType(kSig_i_v);

  let g1 = builder.addImportedGlobal("q", "base", kWasmI32);

  let a = builder.addImport("q", "exp_add", sig_i_ii);
  let i = builder.addImport("q", "exp_inc", sig_i_i);
  let t = builder.addImport("q", "exp_ten", sig_i_v);

  builder.setFunctionTableBounds(7, 35);
  // builder.addFunctionTableInit(g1, true,
  //     [funcs.mul.index, funcs.add.index, funcs.sub.index]);
  builder.addFunctionTableInit(g1, true, [a, i, t]);

  builder.addExportOfKind("table", kExternalTable, 0);
  let module = new WebAssembly.Module(builder.toBuffer());

  function exp_add(a, b) { return a + b; }
  function exp_inc(a) { return a + 1 | 0; }
  function exp_ten() { return 10; }

  let instance = new WebAssembly.Instance(module, {q: {base: 0,
    exp_add: exp_add, exp_inc: exp_inc, exp_ten: exp_ten}});

  let table = instance.exports.table;
  exp_a = table.get(0);
  exp_i = table.get(1);
  exp_t = table.get(2);

  assertEquals(exp_a(1, 4), 5);
  assertEquals(exp_i(8), 9);
  assertEquals(exp_t(0), 10);

  let builder1 = new WasmModuleBuilder();
  let g = builder1.addImportedGlobal("q", "base", kWasmI32);
  let funcs = addFunctions(builder1);

  builder1.addImportedTable("q", "table", 6, 36);
  builder1.addFunctionTableInit(g, true,
      [funcs.mul.index, funcs.add.index, funcs.sub.index]);
  let module1 = new WebAssembly.Module(builder1.toBuffer());

  function verifyTableFuncs(base) {
    assertEquals(exp_a(1, 4), 5);
    assertEquals(exp_i(8), 9);
    assertEquals(exp_t(0), 10);

    mul = table.get(base);
    add = table.get(base + 1);
    sub = table.get(base + 2);

    assertEquals(20, mul(10, 2));
    assertEquals(12, add(10, 2));
    assertEquals(8, sub(10, 2));
  }

  for (let i = 3; i < 10; i++) {
    let instance1 = new WebAssembly.Instance(module1, {q: {base: i, table: table}});
    verifyTableFuncs(i);
    assertEquals(table.length, table.grow(3));
    verifyTableFuncs(i);

    assertThrows(() => table.set(table.length, id), RangeError);
    assertThrows(() => table.set(table.length + 5, id), RangeError);
  }
})();

(function ModulesInstancesSharedTableBoundsCheck() {
  print("ModulesInstancesSharedTableBoundsCheck");
  let table = new WebAssembly.Table({element: "anyfunc",
    initial: 1, maximum: kMaxTableSize});

  function CallModuleBuilder() {
    var builder = new WasmModuleBuilder();
    builder.addType(kSig_i_v);
    builder.addType(kSig_v_v);
    let index_i_ii = builder.addType(kSig_i_ii);
    let index_i_i = builder.addType(kSig_i_i);
    builder.addImportedTable("x", "table", 1, kMaxTableSize);
    builder.addFunction("add", index_i_ii)
      .addBody([
        kExprGetLocal, 0,
        kExprGetLocal, 1,
        kExprI32Add]);
    builder.addFunction("main", index_i_i)
      .addBody([
        kExprI32Const, 5,
        kExprI32Const, 5,
        kExprGetLocal, 0,
        kExprCallIndirect, index_i_ii, kTableZero])
      .exportAs("main");
    builder.addFunctionTableInit(0, false, [0], true);
    return new WebAssembly.Module(builder.toBuffer());
  }

  var instances = [], modules = [];
  modules[0] = CallModuleBuilder();
  modules[1] = CallModuleBuilder();

  // Modules[0] shared by instances[0..2], modules[1] shared by instances[3, 4]
  instances[0] = new WebAssembly.Instance(modules[0], {x: {table:table}});
  instances[1] = new WebAssembly.Instance(modules[0], {x: {table:table}});
  instances[2] = new WebAssembly.Instance(modules[0], {x: {table:table}});
  instances[3] = new WebAssembly.Instance(modules[1], {x: {table:table}});
  instances[4] = new WebAssembly.Instance(modules[1], {x: {table:table}});

  function VerifyTableBoundsCheck(size) {
    print("Verifying bounds for size = " + size);
    assertEquals(size, table.length);
    for (let i = 0; i < 5; i++) {
      // Sanity check for indirect call
      assertEquals(10, instances[i].exports.main(0));
      // Bounds check at different out of bounds indices
      assertInvalidFunction = function(s) {
        assertThrows(
            () => instances[i].exports.main(s), WebAssembly.RuntimeError,
            /invalid function/);
      }
      assertInvalidFunction(size);
      assertInvalidFunction(size + 1);
      assertInvalidFunction(size + 1000);
      assertInvalidFunction(2 * size);
    }
  }

  for (let i = 0; i < 4; i++) {
    VerifyTableBoundsCheck(99900 * i + 1);
    table.grow(99900);
  }
})();
