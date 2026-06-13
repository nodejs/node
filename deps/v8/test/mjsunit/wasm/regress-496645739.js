// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let sig_v_v = builder.addType(kSig_v_v);
let cont_v_v = builder.addCont(sig_v_v);

let tag0 = builder.addTag(kSig_v_v);

builder.startRecGroup();
let next = builder.nextTypeIndex();
let sig_f1 = builder.addType(makeSig([wasmRefNullType(next + 1), wasmRefType(cont_v_v)], []));
let cont_f1 = builder.addCont(sig_f1);
builder.endRecGroup();

let f1 = builder.addFunction("f1", sig_f1)
    .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 0,
        kExprRefAsNonNull,
        kExprResume, ...wasmUnsignedLeb(cont_f1), 0,
    ]).exportFunc();

builder.addDeclarativeElementSegment([f1.index]);

let main_inner = builder.addFunction("main_inner", kSig_v_v)
    .addBody([
        kExprRefFunc, f1.index,
        kExprContNew, ...wasmUnsignedLeb(cont_f1),
        kExprLocalSet, 0,
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kExprSwitch, ...wasmUnsignedLeb(cont_f1), ...wasmUnsignedLeb(tag0),
    ]).addLocals(wasmRefType(cont_f1), 1).exportFunc();

builder.addDeclarativeElementSegment([main_inner.index]);

builder.addFunction("main", kSig_v_v)
    .addBody([
        kExprBlock, kWasmRef, cont_v_v,
          kExprRefFunc, main_inner.index,
          kExprContNew, ...wasmUnsignedLeb(cont_v_v),
          kExprResume, ...wasmUnsignedLeb(cont_v_v), 1,
            kOnSwitch, ...wasmUnsignedLeb(tag0),
          kExprReturn,
        kExprEnd,
        kExprDrop,
    ]).exportFunc();

let instance = builder.instantiate();
assertThrows(() => instance.exports.main(), WebAssembly.RuntimeError);
