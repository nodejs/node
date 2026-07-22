// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-wasm-globals --no-liftoff

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();

let global_i32 = builder.addGlobal(kWasmI32, true, false);
let global_i64 = builder.addGlobal(kWasmI64, true, false)
let global_f32 = builder.addGlobal(kWasmF32, true, false);
let global_f64 = builder.addGlobal(kWasmF64, true, false)
let global_s128 = builder.addGlobal(kWasmS128, true, false);

builder.addFunction('set_i32', kSig_v_i).exportFunc()
    .addBody([kExprLocalGet, 0, kExprGlobalSet, global_i32.index]);
builder.addFunction('get_i32', kSig_v_v).exportFunc()
    .addBody([kExprGlobalGet, global_i32.index, kExprDrop]);

builder.addFunction('set_f32', kSig_v_f).exportFunc()
    .addBody([kExprLocalGet, 0, kExprGlobalSet, global_f32.index]);
builder.addFunction('get_f32', kSig_v_v).exportFunc()
    .addBody([kExprGlobalGet, global_f32.index, kExprDrop]);

builder.addFunction('set_i64', kSig_v_l).exportFunc()
    .addBody([kExprLocalGet, 0, kExprGlobalSet, global_i64.index]);
builder.addFunction('get_i64', kSig_v_v).exportFunc()
    .addBody([kExprGlobalGet, global_i64.index, kExprDrop]);

builder.addFunction('set_f64', kSig_v_d).exportFunc()
    .addBody([kExprLocalGet, 0, kExprGlobalSet, global_f64.index]);
builder.addFunction('get_f64', kSig_v_v).exportFunc()
    .addBody([kExprGlobalGet, global_f64.index, kExprDrop]);

builder.addFunction('set_s128', kSig_v_i).exportFunc()
    .addBody([
        kExprLocalGet, 0,
        kSimdPrefix, kExprI32x4Splat,
        kExprGlobalSet, global_s128.index
    ]);
builder.addFunction('get_s128', kSig_v_v).exportFunc()
    .addBody([kExprGlobalGet, global_s128.index, kExprDrop]);

var module = builder.instantiate();

module.exports.set_i32(42);
module.exports.get_i32();

module.exports.set_f32(3.14);
module.exports.get_f32();

module.exports.set_i64(1234567890123n);
module.exports.get_i64();

module.exports.set_f64(2.71828);
module.exports.get_f64();

module.exports.set_s128(0x12345678);
module.exports.get_s128();
