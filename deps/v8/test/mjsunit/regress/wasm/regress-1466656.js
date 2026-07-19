// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

// We were hitting this DCHECK in assembler-arm64.cc:
//    DCHECK(IsImmLSUnscaled(addr.offset()));
// because the tiering budget array is accessed at offset 16384 for function
// 4096, and that offset is neiter `IsImmLSScaled` or `IsImmLSUnscaled`.
const num_functions = 4097;
for (let j = 0; j < num_functions; ++j) {
  builder.addFunction(undefined, kSig_v_v)
    .addBody([]);
}
builder.toModule();
