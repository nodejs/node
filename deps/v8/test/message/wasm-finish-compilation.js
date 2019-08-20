// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-opt

load('test/mjsunit/wasm/wasm-module-builder.js');

// Test that d8 does not terminate until wasm compilation has finished and the
// promise was resolved.

var builder = new WasmModuleBuilder();
// Add a few functions so it takes some time to compile.
for (var i = 0; i < 2000; ++i) {
  builder.addFunction('fun' + i, kSig_i_v)
      .addBody([...wasmI32Const(i)])
      .exportFunc();
}
builder.asyncInstantiate()
    .then(instance => instance.exports.fun1155())
    .then(res => print('Result of executing fun1155: ' + res));
