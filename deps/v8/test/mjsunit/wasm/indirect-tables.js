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

  let d = builder.addImport("js_div", kSig_i_ii);
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
  let g = builder.addImportedGlobal("base", undefined, kAstI32);
  builder.addFunctionTableInit(g, true, [f.mul.index, f.add.index,
                                         f.sub.index,
                                         d]);
  builder.addExportOfKind("table", kExternalTable, 0);

  let module = new WebAssembly.Module(builder.toBuffer());

  for (let i = 0; i < 5; i++) {
    print(" base = " + i);
    let instance = new WebAssembly.Instance(module, {base: i, js_div: js_div});
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
    assertEquals("blarg", add.name);

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

  let d = builder.addImport("js_div", kSig_i_ii);
  let f = AddFunctions(builder);
  builder.setFunctionTableLength(kTableSize);
  let g = builder.addImportedGlobal("base", undefined, kAstI32);
  builder.addFunctionTableInit(g, true, [f.mul.index, f.add.index,
                                         f.sub.index,
                                         d]);
  builder.addExportOfKind("table", kExternalTable, 0);

  let m1 = new WebAssembly.Module(builder.toBuffer());

  var builder = new WasmModuleBuilder();

  builder.addImportedTable("table", undefined, kTableSize, kTableSize);
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
    let i1 = new WebAssembly.Instance(m1, {base: i, js_div: js_div});
    let table = i1.exports.table;
    assertEquals(10, table.length);
    let i2 = new WebAssembly.Instance(m2, {table: table});
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

  let d = builder.addImport("js_div", kSig_i_ii);
  builder.addImportedTable("table", undefined, kTableSize, kTableSize);
  let g = builder.addImportedGlobal("base", undefined, kAstI32);
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
                                       initial: kTableSize});
    assertEquals(10, table.length);
    let i2 = new WebAssembly.Instance(m2, {base: i, table: table,
                                           js_div: js_div});
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
  let table = new WebAssembly.Table({element: "anyfunc", initial: 10});

  var builder = new WasmModuleBuilder();

  builder.addImportedTable("table", undefined, kTableSize, kTableSize);
  let g = builder.addImportedGlobal("base", undefined, kAstI32);
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
    let instance = new WebAssembly.Instance(module, {base: i, table: table});

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

  builder.setFunctionTableLength(kTableSize);
  builder.addFunctionTableInit(1, false, [f2.index]);
  builder.addImportedTable("table", undefined, kTableSize, kTableSize);

  var m2 = new WebAssembly.Module(builder.toBuffer());

  assertFalse(sig_index1 == sig_index2);

  var i1 = new WebAssembly.Instance(m1);
  var i2 = new WebAssembly.Instance(m2, {table: i1.exports.table});

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
      builder.addImportedTable("impfoo", undefined, impsize, impsize);

      let m2 = new WebAssembly.Module(builder.toBuffer());

      var i1 = new WebAssembly.Instance(m1);

      // TODO(titzer): v8 currently requires import table size to match
      // export table size.
      var ffi = {impfoo: i1.exports.expfoo};
      if (expsize == impsize) {
        var i2 = new WebAssembly.Instance(m2, ffi);
      } else {
        assertThrows(() => new WebAssembly.Instance(m2, ffi));
      }
    }
  }



})();
