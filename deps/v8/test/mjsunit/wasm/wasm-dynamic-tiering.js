// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-dynamic-tiering --liftoff
// Flags: --no-wasm-tier-up
// Make the test faster:
// Flags: --wasm-tiering-budget=1000

// This test busy-waits for tier-up to be complete, hence it does not work in
// predictable more where we only have a single thread.
// Flags: --no-predictable

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const num_functions = 2;

const builder = new WasmModuleBuilder();
for (let i = 0; i < num_functions; ++i) {
  let kFunction = builder.addFunction('f' + i, kSig_i_v)
    .addBody(wasmI32Const(i))
    .exportAs('f' + i)
}

let instance = builder.instantiate();

// The first few calls happen with Liftoff code.
for (let i = 0; i < 3; ++i) {
  instance.exports.f0();
  instance.exports.f1();
}
assertTrue(%IsLiftoffFunction(instance.exports.f0));
assertTrue(%IsLiftoffFunction(instance.exports.f1));

// Keep calling the function until it gets tiered up.
while (%IsLiftoffFunction(instance.exports.f1)) {
  instance.exports.f1();
}
assertTrue(%IsLiftoffFunction(instance.exports.f0));
