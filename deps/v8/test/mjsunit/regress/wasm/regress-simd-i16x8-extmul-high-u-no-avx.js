// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-enable-avx

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("main", kSig_i_i)
  .addLocals(kWasmS128, 1)
  .addBody([
    kExprLocalGet, 0,
    ...SimdInstr(kExprI8x16Splat),
    kExprLocalSet, 1,
    kExprLocalGet, 1,
    kExprLocalGet, 1,
    ...SimdInstr(kExprI16x8ExtMulHighI8x16U),
    ...SimdInstr(kExprI16x8ExtractLaneU), 0,
  ])
  .exportFunc();

let instance = builder.instantiate();
assertEquals(4, instance.exports.main(2));
assertEquals(65025, instance.exports.main(-1));
