// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-bulk-memory

load("test/mjsunit/wasm/wasm-module-builder.js");

function addFunction(builder, k) {
  let m = builder.addFunction("", kSig_i_v)
      .addBody([...wasmI32Const(k)]);
  return m;
}

function assertCall(call, ...elems) {
  for (var i = 0; i < elems.length; i++) {
    assertEquals(elems[i], call(i));
  }
}

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
            kExprGlobalGet, g,
            ...wasmI32Const(i),
            kExprI32Add
          ]);
      f.exportAs(`f${i}`);
    }

    builder.addFunction("copy", sig_v_iii)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kNumericPrefix, kExprTableCopy, kTableZero, kTableZero])
      .exportAs("copy");

    builder.addFunction("call", sig_i_i)
      .addBody([
        kExprLocalGet, 0,
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
