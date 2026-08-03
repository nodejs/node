// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-wasm-functions=1000100

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig_index = builder.addType(makeSig([kWasmI32], [kWasmI32]));

for (let j = 0; j < 1000010; ++j) {
  builder.addFunction(undefined, sig_index)
    .addBody([kExprLocalGet, 0]);
}
const instance = builder.instantiate();
