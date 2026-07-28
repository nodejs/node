// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax --dump-counters

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
let func_sig = builder.addType(makeSig([], []));
let inlinee = builder.addFunction('inlinee', func_sig).addBody([]).exportFunc();
builder.addFunction('f', func_sig)
    .addBody([kExprI32Const, 0, kExprCallIndirect, func_sig, kTableZero])
    .exportFunc();
let table = builder.addTable(kWasmFuncRef, 1);
builder.addActiveElementSegment(
    table.index, wasmI32Const(0), [[kExprRefFunc, inlinee.index]],
    kWasmFuncRef);
let exports = builder.instantiate().exports;
exports.f();
gc();
%WasmTierUpFunction(exports.f);
