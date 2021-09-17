// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc-experiments

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();
let struct1 = builder.addStruct([makeField(kWasmI32, true)]);
let struct2 = builder.addStructExtending(
    [makeField(kWasmI32, true), makeField(kWasmI32, true)], struct1);

let array1 = builder.addArray(kWasmI32, true);
let array2 = builder.addArrayExtending(kWasmI32, true, array1);

builder.addFunction("main", kSig_v_v)
    .addLocals(wasmOptRefType(struct1), 1)
    .addLocals(wasmOptRefType(array1), 1)
    .addBody([
        kGCPrefix, kExprRttCanon, struct2,
        kGCPrefix, kExprStructNewDefault, struct2,
        kExprLocalSet, 0,
        kExprI32Const, 10,  // length
        kGCPrefix, kExprRttCanon, array2,
        kGCPrefix, kExprArrayNewDefault, array2,
        kExprLocalSet, 1
    ]);

// This test is only interested in type checking.
builder.instantiate();
