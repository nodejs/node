// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --stress-wasm-memory-moving --no-wasm-trap-handler

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
const mem = builder.addMemory(1, 10000, false);

let sig_v_v = builder.addType(kSig_v_v);
let cont_v_v = builder.addCont(sig_v_v);
let sig_v_c = builder.addType(makeSig([wasmRefNullType(cont_v_v)], []));
let cont_v_c = builder.addCont(sig_v_c);
let sig_v_c2 = builder.addType(makeSig([wasmRefNullType(cont_v_c)], []));
let cont_v_c2 = builder.addCont(sig_v_c2);

let tag = builder.addTag(kSig_v_v);
let g = builder.addGlobal(kWasmI32, true);

let grower = builder.addFunction("grower", sig_v_c2).addBody([
  ...wasmI32Const(500), kExprMemoryGrow, mem, kExprDrop,
  kExprLocalGet, 0, kExprSwitch, cont_v_c, tag, kExprUnreachable,
]);

let reader = builder.addFunction("reader", sig_v_c).addBody([
  kExprLocalGet, 0, kExprDrop,
  kExprRefFunc, grower.index, kExprContNew, cont_v_c2,
  kExprSwitch, cont_v_c2, tag, kExprDrop,
  // stale mem_start_ → load from freed buffer
  kExprI32Const, 0, kExprI32LoadMem, 2, 0,
  kExprGlobalSet, g.index,
]);

builder.addDeclarativeElementSegment([grower.index, reader.index]);

builder.addFunction("main", kSig_v_v).addBody([
  kExprRefNull, cont_v_v,
  kExprRefFunc, reader.index, kExprContNew, cont_v_c,
  kExprResume, cont_v_c, 1, kOnSwitch, tag,
]).exportFunc();

builder.instantiate().exports.main();
