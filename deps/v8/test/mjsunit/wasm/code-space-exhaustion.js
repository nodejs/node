// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-max-code-space=1

load('test/mjsunit/wasm/wasm-module-builder.js');

// We only have 1 MB code space. This is enough for the code below, but for all
// 1000 modules, it requires several GCs to get rid of the old code.
const builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_i_i).addBody([kExprLocalGet, 0]);
const buffer = builder.toBuffer();

for (let i = 0; i < 1000; ++i) {
  new WebAssembly.Module(buffer);
}
