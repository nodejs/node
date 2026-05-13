// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --verify-heap

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let b = new WasmModuleBuilder();

let kWasmSharedExternRef = wasmRefType(kWasmExternRef).shared();
let kWasmSharedNullExternRef = wasmRefNullType(kWasmExternRef).shared();

let sig_i_e = b.addType(makeSig([kWasmI32], [kWasmSharedExternRef]));
let array_type = b.addArray(kWasmSharedNullExternRef,
                            {mutable: true, shared: true});
let sig_i_a = b.addType(makeSig([kWasmI32], [wasmRefType(array_type)]));

b.addImport("wasm:js-string", "fromCharCode", sig_i_e);
b.addFunction("test", sig_i_a)
  .addLocals(wasmRefType(array_type), 1)
  .addBody([
    ...wasmI32Const(1),
    kGCPrefix, kExprArrayNewDefault, array_type,
    kExprLocalTee, 1,
    kExprI32Const, 0,
    kExprLocalGet, 0,
    kExprCallFunction, 0,
    kGCPrefix, kExprArraySet, array_type,
    kExprLocalGet, 1,
  ])
  .exportFunc();

let instance = b.instantiate({}, { builtins: ["js-string"] });
instance.exports.test(0x100);
instance.exports.test(0xff);
