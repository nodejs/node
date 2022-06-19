// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --liftoff-only --trace-wasm-memory

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 17);
// Generate function 1 (out of 3).
builder.addFunction('load', kSig_i_v)
    .addBody([
      // body:
      kExprI32Const, 0,         // i32.const
      kExprI32LoadMem8U, 0, 5,  // i32.load8_u
    ])
    .exportFunc();
const instance = builder.instantiate();
instance.exports.load();
