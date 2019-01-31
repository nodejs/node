// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-bulk-memory

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestTableCopyInbounds() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_v_iii = builder.addType(kSig_v_iii);
  let kTableSize = 5;

  builder.setTableBounds(kTableSize, kTableSize);

  builder.addFunction("copy", sig_v_iii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 2,
      kNumericPrefix, kExprTableCopy, kTableZero])
    .exportAs("copy");

  let instance = builder.instantiate();
  let copy = instance.exports.copy;
  for (let i = 0; i < kTableSize; i++) {
    copy(0, 0, i); // nop
    copy(0, i, kTableSize - i);
    copy(i, 0, kTableSize - i);
  }
  let big = 1000000;
  copy(big, 0, 0); // nop
  copy(0, big, 0); // nop
})();

function addFunction(builder, k) {
  let m = builder.addFunction("", kSig_i_v)
      .addBody([...wasmI32Const(k)]);
  return m;
}

function addFunctions(builder, count) {
  let o = {};
  for (var i = 0; i < count; i++) {
    o[`f${i}`] = addFunction(builder, i);
  }
  return o;
}

function assertTable(obj, ...elems) {
  for (var i = 0; i < elems.length; i++) {
    assertEquals(elems[i], obj.get(i));
  }
}

(function TestTableCopyElems() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let sig_v_iii = builder.addType(kSig_v_iii);
  let kTableSize = 5;

  builder.setTableBounds(kTableSize, kTableSize);

  {
    let o = addFunctions(builder, kTableSize);
    builder.addElementSegment(0, false,
       [o.f0.index, o.f1.index, o.f2.index, o.f3.index, o.f4.index]);
  }

  builder.addFunction("copy", sig_v_iii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 2,
      kNumericPrefix, kExprTableCopy, kTableZero])
    .exportAs("copy");

  builder.addExportOfKind("table", kExternalTable, 0);

  let instance = builder.instantiate();
  let table = instance.exports.table;
  let f0 = table.get(0), f1 = table.get(1), f2 = table.get(2),
      f3 = table.get(3), f4 = table.get(4);
  let copy = instance.exports.copy;

  assertEquals(0, f0());
  assertEquals(1, f1());
  assertEquals(2, f2());

  assertTable(table, f0, f1, f2, f3, f4);
  copy(0, 1, 1);
  assertTable(table, f1, f1, f2, f3, f4);
  copy(0, 1, 2);
  assertTable(table, f1, f2, f2, f3, f4);
  copy(3, 0, 2);
  assertTable(table, f1, f2, f2, f1, f2);
  copy(1, 0, 2);
  assertTable(table, f1, f1, f2, f1, f2);
})();

function assertCall(call, ...elems) {
  for (var i = 0; i < elems.length; i++) {
    assertEquals(elems[i], call(i));
  }
}

(function TestTableCopyCalls() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let sig_v_iii = builder.addType(kSig_v_iii);
  let sig_i_i = builder.addType(kSig_i_i);
  let sig_i_v = builder.addType(kSig_i_v);
  let kTableSize = 5;

  builder.setTableBounds(kTableSize, kTableSize);

  {
    let o = addFunctions(builder, 5);
    builder.addElementSegment(0, false,
       [o.f0.index, o.f1.index, o.f2.index, o.f3.index, o.f4.index]);
  }

  builder.addFunction("copy", sig_v_iii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 2,
      kNumericPrefix, kExprTableCopy, kTableZero])
    .exportAs("copy");

  builder.addFunction("call", sig_i_i)
    .addBody([
      kExprGetLocal, 0,
      kExprCallIndirect, sig_i_v, kTableZero])
    .exportAs("call");

  let instance = builder.instantiate();
  let copy = instance.exports.copy;
  let call = instance.exports.call;

  assertCall(call, 0, 1, 2, 3, 4);
  copy(0, 1, 1);
  assertCall(call, 1, 1, 2, 3, 4);
  copy(0, 1, 2);
  assertCall(call, 1, 2, 2, 3, 4);
  copy(3, 0, 2);
  assertCall(call, 1, 2, 2, 1, 2);
})();

(function TestTableCopyOob1() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let sig_v_iii = builder.addType(kSig_v_iii);
  let kTableSize = 5;

  builder.setTableBounds(kTableSize, kTableSize);

  builder.addFunction("copy", sig_v_iii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 2,
      kNumericPrefix, kExprTableCopy, kTableZero])
    .exportAs("copy");

  let instance = builder.instantiate();
  let copy = instance.exports.copy;
  copy(0, 0, 1); // nop
  copy(0, 0, kTableSize); // nop
  assertThrows(() => copy(0, 0, kTableSize+1));
  assertThrows(() => copy(1, 0, kTableSize));
  assertThrows(() => copy(0, 1, kTableSize));

  for (let big = 4294967295; big > 1000; big >>>= 1) {
    assertThrows(() => copy(big, 0, 1));
    assertThrows(() => copy(0, big, 1));
    assertThrows(() => copy(0, 0, big));
  }

  for (let big = -1000; big != 0; big <<= 1) {
    assertThrows(() => copy(big, 0, 1));
    assertThrows(() => copy(0, big, 1));
    assertThrows(() => copy(0, 0, big));
  }
})();

(function TestTableCopyShared() {
  print(arguments.callee.name);
  let kTableSize = 5;

  let table = new WebAssembly.Table({element: "anyfunc",
                                     initial: kTableSize,
                                     maximum: kTableSize});

  let module = (() => {
    let builder = new WasmModuleBuilder();
    let sig_v_iii = builder.addType(kSig_v_iii);
    let sig_i_i = builder.addType(kSig_i_i);
    let sig_i_v = builder.addType(kSig_i_v);

    builder.addImportedTable("m", "table", kTableSize, kTableSize);
    var g = builder.addImportedGlobal("m", "g", kWasmI32);

    for (let i = 0; i < kTableSize; i++) {
      let f = builder.addFunction("", kSig_i_v)
          .addBody([
            kExprGetGlobal, g,
            ...wasmI32Const(i),
            kExprI32Add
          ]);
      f.exportAs(`f${i}`);
    }

    builder.addFunction("copy", sig_v_iii)
      .addBody([
        kExprGetLocal, 0,
        kExprGetLocal, 1,
        kExprGetLocal, 2,
        kNumericPrefix, kExprTableCopy, kTableZero])
      .exportAs("copy");

    builder.addFunction("call", sig_i_i)
      .addBody([
        kExprGetLocal, 0,
        kExprCallIndirect, sig_i_v, kTableZero])
      .exportAs("call");

    return builder.toModule();
  })();

  // Two different instances with different globals, to verify that
  // dispatch tables get updated with the right instance.
  let x = new WebAssembly.Instance(module, {m: {g: 1000, table: table}});
  let y = new WebAssembly.Instance(module, {m: {g: 2000, table: table}});

  let x_call = x.exports.call;
  let y_call = y.exports.call;

  assertNotEquals(x.exports.f3, y.exports.f3);

  table.set(0, x.exports.f0);
  table.set(1, x.exports.f1);
  table.set(2, x.exports.f2);
  table.set(3, y.exports.f3);
  table.set(4, y.exports.f4);

  assertEquals(2003, table.get(3)(3));
  assertEquals(2003, x_call(3));
  assertEquals(2003, y_call(3));

  // Check that calling copy on either of them updates the dispatch table
  // on both of them.
  assertCall(x_call, 1000, 1001, 1002, 2003, 2004);
  assertCall(y_call, 1000, 1001, 1002, 2003, 2004);

  x.exports.copy(0, 1, 1);

  assertCall(x_call, 1001, 1001, 1002, 2003, 2004);
  assertCall(y_call, 1001, 1001, 1002, 2003, 2004);

  y.exports.copy(0, 1, 2);

  assertCall(x_call, 1001, 1002, 1002, 2003, 2004);
  assertCall(y_call, 1001, 1002, 1002, 2003, 2004);

  x.exports.copy(3, 0, 2);

  assertCall(x_call, 1001, 1002, 1002, 1001, 1002);
  assertCall(y_call, 1001, 1002, 1002, 1001, 1002);
})();
