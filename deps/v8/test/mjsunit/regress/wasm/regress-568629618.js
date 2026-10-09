// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboshaft-verify-load-elimination --wasm-shared

// Run with --no-liftoff to reproduce (not included in flags as it is included
// with --variants=stress).

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let workerScript = `
  onmessage = function(msg) {
    let struct = msg.data.struct;
    let module = msg.data.module;
    let instance = new WebAssembly.Instance(module);
    let setFn = instance.exports.set;
    let i = 0;
    while (true) {
      setFn(struct, i++);
    }
  };
`;

let builder = new WasmModuleBuilder();
let struct_type = builder.addStruct({fields: [makeField(kWasmI32, true)],
                                     shared: true});
let kSig_i_my_struct = makeSig([wasmRefNullType(struct_type)], [kWasmI32]);

builder.addFunction("test", kSig_i_my_struct)
  .addLocals(kWasmI32, 1)
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprStructGet, struct_type, 0,

    // Delay loop
    kExprI32Const, 0,
    kExprLocalSet, 1,
    kExprLoop, kWasmVoid,
      kExprLocalGet, 1,
      kExprI32Const, 1,
      kExprI32Add,
      kExprLocalSet, 1,
      kExprLocalGet, 1,
      ...wasmI32Const(5000),
      kExprI32LtS,
      kExprBrIf, 0,
    kExprEnd,

    kExprLocalGet, 0,
    kGCPrefix, kExprStructGet, struct_type, 0,

    kExprI32Add
  ])
  .exportFunc();

builder.addFunction("alloc", makeSig([], [wasmRefNullType(struct_type)]))
  .addBody([
    kExprI32Const, 10,
    kGCPrefix, kExprStructNew, struct_type
  ])
  .exportFunc();

builder.addFunction("set",
                    makeSig([wasmRefNullType(struct_type), kWasmI32], []))
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kGCPrefix, kExprStructSet, struct_type, 0
  ])
  .exportFunc();

let module = builder.toModule();
let instance = new WebAssembly.Instance(module);
let struct = instance.exports.alloc();

let worker = new Worker(workerScript, {type: 'string'});
worker.postMessage({struct: struct, module: module});

let testFn = instance.exports.test;
for (let i = 0; i < 10000; i++) {
  testFn(struct);
}
print("Done");
