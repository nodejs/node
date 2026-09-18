// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const arrT = builder.addArray(kWasmAnyRef);

let fill_sig = makeSig(
    [kWasmI32, kWasmI32, kWasmI32, kWasmI32, wasmRefType(arrT), kWasmAnyRef],
    []);
builder.addFunction('fill', fill_sig).exportFunc().addBody([
    kExprLocalGet, 4,
    kExprLocalGet, 0,
    kExprI32Eqz,
    kExprLocalGet, 5,
    kExprI32Const, 1,
    kGCPrefix, kExprArrayFill, arrT,
  ]);

builder.addFunction('mkarr', makeSig([kWasmI32], [kWasmAnyRef])).exportFunc()
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprArrayNewDefault, arrT,
  ]);

const instance = builder.instantiate();

const arr = instance.exports.mkarr(4);
gc();  // Promote {arr} to old space.
gc();

instance.exports.fill(1, 0, 0, 0, arr, {});
