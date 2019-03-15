// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-bulk-memory

load("test/mjsunit/wasm/wasm-module-builder.js");

function addFunction(builder, k) {
  let m = builder.addFunction("", kSig_i_v)
      .addBody([...wasmI32Const(k)]);
  return m;
}

function addFunctions(builder, count, exportf = false) {
  let o = {};
  for (var i = 0; i < count; i++) {
    let name = `f${i}`;
    o[name] = addFunction(builder, i);
    if (exportf) o[name].exportAs(name);
  }
  return o;
}

function assertTable(obj, ...elems) {
  for (var i = 0; i < elems.length; i++) {
    assertEquals(elems[i], obj.get(i));
  }
}

(function TestTableInitInBounds() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_v_iii = builder.addType(kSig_v_iii);
  let kTableSize = 5;

  builder.setTableBounds(kTableSize, kTableSize);
  {
    let o = addFunctions(builder, kTableSize, true);
    builder.addPassiveElementSegment(
       [o.f0.index, o.f1.index, o.f2.index, o.f3.index, o.f4.index, null]);
  }

  builder.addFunction("init0", sig_v_iii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 2,
      kNumericPrefix, kExprTableInit, kSegmentZero, kTableZero])
    .exportAs("init0");

  builder.addExportOfKind("table", kExternalTable, 0);

  let instance = builder.instantiate();
  let x = instance.exports;

  assertTable(x.table, null, null, null, null, null);

  // test actual writes.
  x.init0(0, 0, 1);
  assertTable(x.table, x.f0, null, null, null, null);
  x.init0(0, 0, 2);
  assertTable(x.table, x.f0, x.f1, null, null, null);
  x.init0(0, 0, 3);
  assertTable(x.table, x.f0, x.f1, x.f2, null, null);
  x.init0(3, 0, 2);
  assertTable(x.table, x.f0, x.f1, x.f2, x.f0, x.f1);
  x.init0(3, 1, 2);
  assertTable(x.table, x.f0, x.f1, x.f2, x.f1, x.f2);
  x.init0(3, 2, 2);
  assertTable(x.table, x.f0, x.f1, x.f2, x.f2, x.f3);
  x.init0(3, 3, 2);
  assertTable(x.table, x.f0, x.f1, x.f2, x.f3, x.f4);

  // test writing null
  x.init0(0, 5, 1);
  assertTable(x.table, null, x.f1, x.f2, x.f3, x.f4);
})();

(function TestTableInitOob() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_v_iii = builder.addType(kSig_v_iii);
  let kTableSize = 5;

  builder.setTableBounds(kTableSize, kTableSize);
  {
    let o = addFunctions(builder, kTableSize);
    builder.addPassiveElementSegment(
       [o.f0.index, o.f1.index, o.f2.index, o.f3.index, o.f4.index]);
  }

  builder.addFunction("init0", sig_v_iii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 2,
      kNumericPrefix, kExprTableInit, kSegmentZero, kTableZero])
    .exportAs("init0");

  builder.addExportOfKind("table", kExternalTable, 0);

  let instance = builder.instantiate();
  let x = instance.exports;

  assertTable(x.table, null, null, null, null, null);

  // 0-count is oob.
  assertThrows(() => x.init0(kTableSize+1, 0, 0));
  assertThrows(() => x.init0(0, kTableSize+1, 0));

  assertThrows(() => x.init0(0, 0, 6));
  assertThrows(() => x.init0(0, 1, 5));
  assertThrows(() => x.init0(0, 2, 4));
  assertThrows(() => x.init0(0, 3, 3));
  assertThrows(() => x.init0(0, 4, 2));
  assertThrows(() => x.init0(0, 5, 1));

  assertThrows(() => x.init0(0, 0, 6));
  assertThrows(() => x.init0(1, 0, 5));
  assertThrows(() => x.init0(2, 0, 4));
  assertThrows(() => x.init0(3, 0, 3));
  assertThrows(() => x.init0(4, 0, 2));
  assertThrows(() => x.init0(5, 0, 1));

  assertThrows(() => x.init0(10, 0, 1));
  assertThrows(() => x.init0(0, 10, 1));
})();
