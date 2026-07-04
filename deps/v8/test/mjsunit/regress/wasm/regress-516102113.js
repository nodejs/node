// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-acquire-release

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();

builder.addImportedMemory('imports', 'memory', 1, 256, false, true);
builder.addFunction("go", kSig_i_i).exportFunc()
  .addLocals(kWasmI64, 2)
  .addLocals(wasmRefType(kWasmI31Ref), 1)
  .addBody([
    ...wasmI64Const(140n),
    kExprLocalSet, 1,
    kExprLocalGet, 1,
    kExprLocalGet, 0,
    kAtomicPrefix, kExprI32AtomicStore, 2, 12,
    kExprLocalGet, 0,        // These useless instructions
    kGCPrefix, kExprRefI31,  // are needed to increase
    kExprLocalSet, 3,        // utilization of registers.
    kExprI64Const, 0,
    kExprLocalSet, 2,
    kExprLocalGet, 2,
    kExprLocalGet, 1,
    kAtomicPrefix, kExprI64AtomicStore8U, 32, kAtomicAcqRel, 0,
    kExprLocalGet, 0,
  ]);

let memory =
    new WebAssembly.Memory({initial: 1n, maximum: 256n, address: 'i64'});

const instance = builder.instantiate({imports: { memory }});

instance.exports.go();
