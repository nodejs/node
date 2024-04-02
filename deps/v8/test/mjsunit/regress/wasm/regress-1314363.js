// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-enable-sse4-1

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, makeSig([], [kWasmI64]))
    .addBody([
      ...wasmF32Const(11.3),  // f32.const
      kExprI64SConvertF32,    // i64.trunc_f32_s
    ])
    .exportAs('main');
let instance = builder.instantiate();
assertEquals(11n, instance.exports.main());
