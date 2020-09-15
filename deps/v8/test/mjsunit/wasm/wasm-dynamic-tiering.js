// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-dynamic-tiering --liftoff
// Flags: --no-wasm-tier-up --no-stress-opt

load('test/mjsunit/wasm/wasm-module-builder.js');

const num_iterations = 4;
const num_functions = 2;

const builder = new WasmModuleBuilder();
for (let i = 0; i < num_functions; ++i) {
  let kFunction = builder.addFunction('f' + i, kSig_i_v)
    .addBody(wasmI32Const(i))
    .exportAs('f' + i)
}

let instance = builder.instantiate();

for (let i = 0; i < num_iterations - 1; ++i) {
  instance.exports.f0();
  instance.exports.f1();
}

assertTrue(%IsLiftoffFunction(instance.exports.f0));
assertTrue(%IsLiftoffFunction(instance.exports.f1));

instance.exports.f1();

// Busy waiting until the function is tiered up.
while (true) {
  if (!%IsLiftoffFunction(instance.exports.f1)) {
    break;
  }
}
assertTrue(%IsLiftoffFunction(instance.exports.f0));
