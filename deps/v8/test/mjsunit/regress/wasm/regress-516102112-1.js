// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wide-arithmetic

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let w0 = builder.addFunction('w0', kSig_i_i).exportFunc();
let $mem0 = builder.addMemory(1, 256);

// func $w0: [kWasmI32] -> [kWasmI32]
w0.addLocals(kWasmI64, 4)  // $var1 - $var4
  .addLocals(kWasmI32, 1)  // $var5
  .addBody([
    kExprI64Const, 9,
    kExprLocalTee, 1,
    kExprLocalGet, 1,
    kExprI64Const, 1,
    kExprLocalGet, 1,
    kNumericPrefix, kExprI64Sub128,
    kExprLocalSet, 3,
    kExprLocalSet, 4,
    kExprLocalGet, 0,
    kExprMemoryGrow, $mem0,
  ]);

const instance = builder.instantiate();
instance.exports.w0();
