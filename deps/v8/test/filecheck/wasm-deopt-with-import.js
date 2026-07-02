// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --wasm-deopt --wasm-inlining --liftoff
// Flags: --allow-natives-syntax --trace-deopt-verbose --trace-wasm-inlining

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addImport("m", "imp", kSig_v_v); // index 0

let sig = builder.addType(kSig_v_v);

// main is the first declared function (index 1)
builder.addFunction("main", makeSig([kWasmI32], []))
    .addBody([
        kExprLocalGet, 0,
        kExprCallIndirect, sig, 0
    ])
    .exportAs("main");

// f1 and f2 are after main
let f1 = builder.addFunction("f1", sig).addBody([]);
let f2 = builder.addFunction("f2", sig).addBody([]);

let table = builder.addTable(kWasmFuncRef, 2);
builder.addActiveElementSegment(0, [kExprI32Const, 0], [f1.index, f2.index]);

let module = builder.toModule();
let instance1 = new WebAssembly.Instance(module, {m: {imp: () => {}}});

// Collect feedback and speculatively inline f1.
instance1.exports.main(0);
// CHECK: function 1: Speculatively inlining call_indirect #0, case #0, to function 2
%WasmTierUpFunction(instance1.exports.main);

let instance2 = new WebAssembly.Instance(module, {m: {imp: () => {}}});
// Triggering deopt, --trace-deopt-verbose should trace that we are allocating
// a feedback vector for main. (see crbug.com/503489820)
// CHECK: deoptimizing main, function index 1
// CHECK: allocating feedback vector for function main [1]
instance2.exports.main(1);
