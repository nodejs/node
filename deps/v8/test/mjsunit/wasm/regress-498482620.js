// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --expose-gc
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let sig_v_v = builder.addType(kSig_v_v);
let cont_v_v = builder.addCont(sig_v_v);
let tag_switch = builder.addTag(kSig_v_v);
let tag_inner = builder.addTag(
    builder.addType(makeSig([kWasmI32], [kWasmExternRef])));
let gc_index = builder.addImport("m", "gc", kSig_v_v);
let g_ref = builder.addGlobal(kWasmExternRef, true).exportAs('g_ref');

let sig_target = builder.addType(
    makeSig([kWasmExternRef, wasmRefType(cont_v_v)], []));
let cont_target = builder.addCont(sig_target);
let sig_bound = builder.addType(makeSig([wasmRefType(cont_v_v)], []));
let cont_bound = builder.addCont(sig_bound);

let target = builder.addFunction("target", sig_target).addBody([
    ...wasmI32Const(0xDEADBEEF),
    kExprSuspend, tag_inner,
    kExprDrop,
]);

let switcher = builder.addFunction("switcher", kSig_v_v).addBody([
    kExprGlobalGet, g_ref.index,
    kExprRefFunc, target.index,
    kExprContNew, cont_target,
    kExprContBind, cont_target, cont_bound,
    kExprSwitch, cont_bound, tag_switch,
]);

builder.addDeclarativeElementSegment([target.index, switcher.index]);

let cont_inner = builder.addCont(
    builder.addType(makeSig([kWasmExternRef], [])));
let handler_sig = builder.addType(
    makeSig([], [kWasmI32, wasmRefType(cont_inner)]));

builder.addFunction("main", kSig_v_v).addBody([
    kExprBlock, kWasmRef, cont_v_v,
      kExprBlock, handler_sig,
        kExprRefFunc, switcher.index,
        kExprContNew, cont_v_v,
        kExprResume, cont_v_v, 2,
          kOnSwitch, tag_switch,
          kOnSuspend, tag_inner, 0,
        kExprUnreachable,
      kExprEnd,
      kExprCallFunction, gc_index,
      kExprDrop,
      kExprDrop,
      kExprReturn,
    kExprEnd,
    kExprDrop,
]).exportFunc();

let instance = builder.instantiate({m: {gc}});
instance.exports.g_ref.value = {};
instance.exports.main();
