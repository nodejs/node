// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

let chain = Promise.resolve();
const builder = new WasmModuleBuilder();
for (let i = 0; i < 50; ++i) {
  builder.addFunction('fun' + i, kSig_i_v)
      .addBody([...wasmI32Const(i)])
      .exportFunc();
}
const buffer = builder.toBuffer();
for (let i = 0; i < 100; ++i) {
  chain = chain.then(() => WebAssembly.instantiate(buffer));
}
chain.then(({module, instance}) => instance.exports.fun1155())
    .then(res => print('Result of executing fun1155: ' + res));
