// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --no-wasm-lazy-compilation --no-enable-avx

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let clobber = builder.addImport("m", "clobber", kSig_v_v);
builder.addFunction("main", kSig_i_i).addBody([
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprLocalGet, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprCallFunction, clobber,
    kSimdPrefix, ...kExprI32x4RelaxedLaneSelect,
    kSimdPrefix, kExprI32x4ExtractLane, 0,
]);
let module = builder.toModule();
