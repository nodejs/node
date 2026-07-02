// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let type0 = builder.addType(kSig_l_v);
let type1 = builder.addCont(type0);
let type2 = builder.addType(makeSig([wasmRefType(type1)], [kWasmI64]));
let type3 = builder.addCont(type2);
let type4 = builder.addType(makeSig([], [kExternRefCode]));
let type5 = builder.addCont(type4);
let type6 = builder.addType(kSig_l_v);
let type7 = builder.addType(kSig_v_v);

let tab = builder.addTable(kExternRefCode, 1, 1).exportAs("tab");
let tag0 = builder.addTag(type6);

let target = builder.addFunction("target", type2).addBody([
  ...wasmI64Const(244834611620561n)
]);

let inner = builder.addFunction("inner", type4).addBody([
  kExprRefFunc, target.index,
  kExprContNew, type3,
  kExprSwitch, type3, tag0,
  kExprRefNull, kExternRefCode,
]);

builder.addFunction("main", type7).exportAs("main").addBody([
  kExprI32Const, 0,
  kExprRefFunc, inner.index,
  kExprContNew, type5,
  kExprResume, type5, 1, kOnSwitch, tag0,
  kExprTableSet, tab.index
]);

builder.addDeclarativeElementSegment([target.index, inner.index]);

assertThrows(
  () => {
    let inst = builder.instantiate();
    inst.exports.main();
    inst.exports.tab.get(0);
  },
  WebAssembly.CompileError, /Type mismatch between tag returns/);
