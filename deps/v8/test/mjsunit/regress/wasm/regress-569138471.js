// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --wasm-eager-tier-up-function=1

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const callee = builder.addFunction('callee', kSig_i_i).addBody([
  kExprLocalGet, 0,
]);
builder.addFunction('main', kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprReturnCall, callee.index,
    ])
    .exportFunc();

assertPromiseResult(
    WebAssembly.instantiateStreaming(Promise.resolve(builder.toBuffer())),
    ({instance}) => assertEquals(42, instance.exports.main(42)));
