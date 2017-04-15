// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

function AddFunctions(builder) {
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

function js_div(a, b) { return (a / b) | 0; }

(function ExportedTableTest() {
  print("ExportedTableTest...");

  let builder = new WasmModuleBuilder();

  let d = builder.addImport("q", "js_div", kSig_i_ii);
  let f = AddFunctions(builder);
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprI32Const, 33,  // --
      kExprGetLocal, 0,   // --
      kExprGetLocal, 1,   // --
      kExprCallIndirect, 0, kTableZero])  // --
    .exportAs("main");

  f.add.exportAs("blarg");

  builder.setFunctionTableLength(10);
  let g = builder.addImportedGlobal("q", "base", kWasmI32);
  builder.addFunctionTableInit(g, true, [f.mul.index, f.add.index,
                                         f.sub.index,
                                         d]);
  builder.addExportOfKind("table", kExternalTable, 0);

  let module = new WebAssembly.Module(builder.toBuffer());

  for (let i = 0; i < 5; i++) {
    print(" base = " + i);
    let instance = new WebAssembly.Instance(module, {q: {base: i, js_div: js_div}});
    main = instance.exports.main;
    let table = instance.exports.table;
    assertTrue(table instanceof WebAssembly.Table);
    assertEquals(10, table.length);
    for (let j = 0; j < i; j++) {
      assertSame(null, table.get(j));
    }
    let mul = table.get(i+0);
    let add = table.get(i+1);
    let sub = table.get(i+2);

    print("  mul=" + mul);
    print("  add=" + add);
    print("  sub=" + sub);
    assertEquals("function", typeof mul);
    assertEquals("function", typeof add);
    assertEquals("function", typeof sub);
    assertEquals(2, mul.length);
    assertEquals(2, add.length);
    assertEquals(2, sub.length);
    assertEquals(String(f.add.index), add.name);

    let exp_div = table.get(i+3);
    assertEquals("function", typeof exp_div);
    print("  js_div=" + exp_div);
    // Should have a new, wrapped version of the import.
    assertFalse(js_div == exp_div);


    for (let j = i + 4; j < 10; j++) {
      assertSame(null, table.get(j));
    }

    assertEquals(-33, mul(-11, 3));
    assertEquals(4444444, add(3333333, 1111111));
    assertEquals(-9999, sub(1, 10000));
    assertEquals(-44, exp_div(-88.1, 2));
  }
})();


(function ImportedTableTest() {
  let kTableSize = 10;
  print("ImportedTableTest...");
  var builder = new WasmModuleBuilder();

  let d = builder.addImport("q", "js_div", kSig_i_ii);
  let f = AddFunctions(builder);
  builder.setFunctionTableLength(kTableSize);
  let g = builder.addImportedGlobal("q", "base", kWasmI32);
  builder.addFunctionTableInit(g, true, [f.mul.index, f.add.index,
                                         f.sub.index,
                                         d]);
  builder.addExportOfKind("table", kExternalTable, 0);

  let m1 = new WebAssembly.Module(builder.toBuffer());

  var builder = new WasmModuleBuilder();

  builder.addImportedTable("r", "table", kTableSize, kTableSize);
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprI32Const, 33,  // --
      kExprGetLocal, 0,   // --
      kExprGetLocal, 1,   // --
      kExprCallIndirect, 0, kTableZero])  // --
    .exportAs("main");

  let m2 = new WebAssembly.Module(builder.toBuffer());

  // Run 5 trials at different table bases.
  for (let i = 0; i < 5; i++) {
    print(" base = " + i);
    let i1 = new WebAssembly.Instance(m1, {q: {base: i, js_div: js_div}});
    let table = i1.exports.table;
    assertEquals(10, table.length);
    let i2 = new WebAssembly.Instance(m2, {r: {table: table}});
    let main = i2.exports.main;

    for (var j = 0; j < i; j++) {
      assertThrows(() => main(0, j));
      assertSame(null, table.get(j));
    }

    // mul
    assertEquals("function", typeof table.get(i+0));
    assertEquals(0, main(0, i+0));
    assertEquals(66, main(2, i+0));

    // add
    assertEquals("function", typeof table.get(i+1));
    assertEquals(33, main(0, i+1));
    assertEquals(38, main(5, i+1));

    // sub
    assertEquals("function", typeof table.get(i+2));
    assertEquals(32, main(1, i+2));
    assertEquals(28, main(5, i+2));

    // div
    assertEquals("function", typeof table.get(i+3));
    assertEquals(8, main(4, i+3));
    assertEquals(3, main(11, i+3));

    for (var j = i + 4; j < (kTableSize + 5); j++) {
      assertThrows(x => main(0, j));
      if (j < kTableSize) assertSame(null, table.get(j));
    }
  }
})();

(function ImportedTableTest() {
  let kTableSize = 10;
  print("ManualTableTest...");

  var builder = new WasmModuleBuilder();

  let d = builder.addImport("q", "js_div", kSig_i_ii);
  builder.addImportedTable("q", "table", kTableSize, kTableSize);
  let g = builder.addImportedGlobal("q", "base", kWasmI32);
  let f = AddFunctions(builder);
  builder.addFunctionTableInit(g, true, [f.mul.index, f.add.index,
                                         f.sub.index,
                                         d]);
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprI32Const, 55,  // --
      kExprGetLocal, 0,   // --
      kExprGetLocal, 1,   // --
      kExprCallIndirect, 0, kTableZero])  // --
    .exportAs("main");

  let m2 = new WebAssembly.Module(builder.toBuffer());

  // Run 5 trials at different table bases.
  for (let i = 0; i < 5; i++) {
    print(" base = " + i);
    let table = new WebAssembly.Table({element: "anyfunc",
                                       initial: kTableSize,
                                       maximum: kTableSize});
    assertEquals(10, table.length);
    let i2 = new WebAssembly.Instance(m2, {q: {base: i, table: table,
                                               js_div: js_div}});
    let main = i2.exports.main;

    for (var j = 0; j < i; j++) {
      assertThrows(() => main(0, j));
      assertSame(null, table.get(j));
    }

    // mul
    assertEquals("function", typeof table.get(i+0));
    assertEquals(0, main(0, i+0));
    assertEquals(110, main(2, i+0));

    // add
    assertEquals("function", typeof table.get(i+1));
    assertEquals(55, main(0, i+1));
    assertEquals(60, main(5, i+1));

    // sub
    assertEquals("function", typeof table.get(i+2));
    assertEquals(54, main(1, i+2));
    assertEquals(50, main(5, i+2));

    // div
    assertEquals("function", typeof table.get(i+3));
    assertEquals(13, main(4, i+3));
    assertEquals(5, main(11, i+3));

    for (var j = i + 4; j < (kTableSize + 5); j++) {
      assertThrows(x => main(0, j));
      if (j < kTableSize) assertSame(null, table.get(j));
    }
  }
})();


(function CumulativeTest() {
  print("CumulativeTest...");

  let kTableSize = 10;
  let table = new WebAssembly.Table(
    {element: "anyfunc", initial: kTableSize, maximum: kTableSize});

  var builder = new WasmModuleBuilder();

  builder.addImportedTable("x", "table", kTableSize, kTableSize);
  let g = builder.addImportedGlobal("x", "base", kWasmI32);
  let sig_index = builder.addType(kSig_i_v);
  builder.addFunction("g", sig_index)
    .addBody([
      kExprGetGlobal, g
    ]);
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprCallIndirect, sig_index, kTableZero])  // --
    .exportAs("main");
  builder.addFunctionTableInit(g, true, [g]);

  let module = new WebAssembly.Module(builder.toBuffer());

  for (var i = 0; i < kTableSize; i++) {
    print(" base = " + i);
    let instance = new WebAssembly.Instance(module, {x: {base: i, table: table}});

    for (var j = 0; j < kTableSize; j++) {
      let func = table.get(j);
      if (j > i) {
        assertSame(null, func);
        assertTraps(kTrapFuncSigMismatch, () => instance.exports.main(j));
      } else {
        assertEquals("function", typeof func);
        assertEquals(j, func());
        assertEquals(j, instance.exports.main(j));
      }
    }
  }
})();

(function TwoWayTest() {
  print("TwoWayTest...");
  let kTableSize = 3;

  // Module {m1} defines the table and exports it.
  var builder = new WasmModuleBuilder();
  builder.addType(kSig_i_i);
  builder.addType(kSig_i_ii);
  var sig_index1 = builder.addType(kSig_i_v);
  var f1 = builder.addFunction("f1", sig_index1)
    .addBody([kExprI32Const, 11]);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,   // --
      kExprCallIndirect, sig_index1, kTableZero])  // --
    .exportAs("main");

  builder.setFunctionTableLength(kTableSize);
  builder.addFunctionTableInit(0, false, [f1.index]);
  builder.addExportOfKind("table", kExternalTable, 0);

  var m1 = new WebAssembly.Module(builder.toBuffer());

  // Module {m2} imports the table and adds {f2}.
  var builder = new WasmModuleBuilder();
  builder.addType(kSig_i_ii);
  var sig_index2 = builder.addType(kSig_i_v);
  var f2 = builder.addFunction("f2", sig_index2)
    .addBody([kExprI32Const, 22]);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,   // --
      kExprCallIndirect, sig_index2, kTableZero])  // --
    .exportAs("main");

  builder.addImportedTable("z", "table", kTableSize, kTableSize);
  builder.addFunctionTableInit(1, false, [f2.index], true);

  var m2 = new WebAssembly.Module(builder.toBuffer());

  assertFalse(sig_index1 == sig_index2);

  var i1 = new WebAssembly.Instance(m1);
  var i2 = new WebAssembly.Instance(m2, {z: {table: i1.exports.table}});

  assertEquals(11, i1.exports.main(0));
  assertEquals(11, i2.exports.main(0));

  assertEquals(22, i1.exports.main(1));
  assertEquals(22, i2.exports.main(1));

  assertThrows(() => i1.exports.main(2));
  assertThrows(() => i2.exports.main(2));
  assertThrows(() => i1.exports.main(3));
  assertThrows(() => i2.exports.main(3));

})();

(function MismatchedTableSize() {
  print("MismatchedTableSize...");
  let kTableSize = 5;

  for (var expsize = 1; expsize < 4; expsize++) {
    for (var impsize = 1; impsize < 4; impsize++) {
      print(" expsize = " + expsize + ", impsize = " + impsize);
      var builder = new WasmModuleBuilder();
      builder.setFunctionTableLength(expsize);
      builder.addExportOfKind("expfoo", kExternalTable, 0);

      let m1 = new WebAssembly.Module(builder.toBuffer());

      var builder = new WasmModuleBuilder();
      builder.addImportedTable("y", "impfoo", impsize, impsize);

      let m2 = new WebAssembly.Module(builder.toBuffer());

      var i1 = new WebAssembly.Instance(m1);

      // TODO(titzer): v8 currently requires import table size to match
      // export table size.
      var ffi = {y: {impfoo: i1.exports.expfoo}};
      if (expsize == impsize) {
        var i2 = new WebAssembly.Instance(m2, ffi);
      } else {
        assertThrows(() => new WebAssembly.Instance(m2, ffi));
      }
    }
  }
})();

(function TableGrowBoundsCheck() {
  print("TableGrowBoundsCheck");
  var kMaxSize = 30, kInitSize = 5;
  let table = new WebAssembly.Table({element: "anyfunc",
    initial: kInitSize, maximum: kMaxSize});
  var builder = new WasmModuleBuilder();
  builder.addImportedTable("x", "table", kInitSize, kMaxSize);
  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module, {x: {base: 1, table: table}});

  for(var i = kInitSize; i < kMaxSize; i+=5) {
    assertEquals(i, table.length);
    for (var j = 0; j < i; j++) table.set(j, null);
    for (var j = 0; j < i; j++) assertEquals(null, table.get(j));
    assertThrows(() => table.set(i, null));
    assertThrows(() => table.get(i));
    assertEquals(i, table.grow(5));
  }
  assertEquals(30, table.length);
  assertThrows(() => table.grow(1));
  assertThrows(() => table.set(kMaxSize, null));
  assertThrows(() => table.get(kMaxSize));
})();

(function CumulativeGrowTest() {
  print("CumulativeGrowTest...");
  let table = new WebAssembly.Table({
    element: "anyfunc", initial: 10, maximum: 30});
  var builder = new WasmModuleBuilder();
  builder.addImportedTable("x", "table", 10, 30);

  let g = builder.addImportedGlobal("x", "base", kWasmI32);
  let sig_index = builder.addType(kSig_i_v);
  builder.addFunction("g", sig_index)
    .addBody([
      kExprGetGlobal, g
    ]);
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprCallIndirect, sig_index, kTableZero])  // --
    .exportAs("main");
  builder.addFunctionTableInit(g, true, [g]);
  let module = new WebAssembly.Module(builder.toBuffer());

  var instances = [];
  for (var i = 0; i < 10; i++) {
    print(" base = " + i);
    instances.push(new WebAssembly.Instance(
        module, {x: {base: i, table: table}}));
  }

  for (var j = 0; j < 10; j++) {
    let func = table.get(j);
    assertEquals("function", typeof func);
    assertEquals(j, func());
    assertEquals(j, instances[j].exports.main(j));
  }

  assertEquals(10, table.grow(10));

  // Verify that grow does not alter function behaviors
  for (var j = 0; j < 10; j++) {
    let func = table.get(j);
    assertEquals("function", typeof func);
    assertEquals(j, func());
    assertEquals(j, instances[j].exports.main(j));
  }

  let new_builder = new WasmModuleBuilder();
  new_builder.addExport("wasm", new_builder.addFunction("", kSig_v_v).addBody([]));
  new_builder.addImportedTable("x", "table", 20, 30);
  let new_module = new WebAssembly.Module(new_builder.toBuffer());
  let instance = new WebAssembly.Instance(new_module, {x: {table: table}});
  let new_func = instance.exports.wasm;

  for (var j = 10; j < 20; j++) {
    table.set(j, new_func);
    let func = table.get(j);
    assertEquals("function", typeof func);
    assertSame(new_func, table.get(j));
  }
  assertThrows(() => table.grow(11));
})();


(function TestImportTooLarge() {
  print("TestImportTooLarge...");
  let builder = new WasmModuleBuilder();
  builder.addImportedTable("t", "t", 1, 2);

  // initial size is too large
  assertThrows(() => builder.instantiate({t: {t: new WebAssembly.Table(
    {element: "anyfunc", initial: 3, maximum: 3})}}));

  // maximum size is too large
  assertThrows(() => builder.instantiate({t: {t: new WebAssembly.Table(
    {element: "anyfunc", initial: 1, maximum: 4})}}));

  // no maximum
  assertThrows(() => builder.instantiate({t: {t: new WebAssembly.Table(
    {element: "anyfunc", initial: 1})}}));
})();

(function TableImportLargerThanCompiled() {
  print("TableImportLargerThanCompiled...");
  var kMaxSize = 30, kInitSize = 5;
  var builder = new WasmModuleBuilder();
  builder.addImportedTable("x", "table", 1, 35);
  let table = new WebAssembly.Table({element: "anyfunc",
    initial: kInitSize, maximum: kMaxSize});
  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module, {x: {base: 1, table: table}});
  for (var i = 0; i < kInitSize; ++i) table.set(i, null);
  for (var i = 0; i < kInitSize; ++i) assertEquals(null, table.get(i));
  assertThrows(() => table.set(kInitSize, null));
})();

(function ModulesShareTableAndGrow() {
  print("ModulesShareTableAndGrow...");
  let module1 = (() => {
    let builder = new WasmModuleBuilder();
    builder.addImportedTable("x", "table", 1, 35);
    return new WebAssembly.Module(builder.toBuffer());
  })();
  let module2 = (() => {
    let builder = new WasmModuleBuilder();
    builder.addImportedTable("x", "table", 2, 40);
    return new WebAssembly.Module(builder.toBuffer());
  })();

  var kMaxSize = 30, kInitSize = 5;
  let table = new WebAssembly.Table({element: "anyfunc",
    initial: kInitSize, maximum: kMaxSize});
  let instance1 = new WebAssembly.Instance(
      module1, {x: {base: 1, table: table}});
  let instance2 = new WebAssembly.Instance(
      module2, {x: {base: 1, table: table}});

  for (var i = 0; i < kInitSize; ++i) table.set(i, null);
  for (var i = 0; i < kInitSize; ++i) assertEquals(null, table.get(i));
  assertThrows(() => table.set(kInitSize, null));
  assertEquals(kInitSize, table.grow(5));
  for (var i = 0; i < 2*kInitSize; ++i) table.set(i, null);
  for (var i = 0; i < 2*kInitSize; ++i) assertEquals(null, table.get(i));
  assertThrows(() => table.set(2*kInitSize, null));
  // Try to grow past imported maximum
  assertThrows(() => table.grow(21));
})();
