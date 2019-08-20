// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
sig1 = makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]);
const imp_idx = builder.addImport('q', 'imp', kSig_i_i);
builder.addExport('exp', imp_idx);
const module = builder.toModule();

function bad(a, b, c, d, e, f, g, h) {
    print(JSON.stringify([a, b, c, d, e, f, g, h]));
}
const instance1 = new WebAssembly.Instance(module, {q: {imp: bad}});
const instance2 = new WebAssembly.Instance(module, {q: {imp: i => i}});

print(instance1.exports.exp(5));
