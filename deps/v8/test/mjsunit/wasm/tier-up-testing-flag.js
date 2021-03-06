// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --liftoff --no-wasm-tier-up
// Compile functions 0 and 2 with Turbofan, the rest with Liftoff:
// Flags: --wasm-tier-mask-for-testing=5

load('test/mjsunit/wasm/wasm-module-builder.js');

const num_functions = 5;

function create_builder() {
  const builder = new WasmModuleBuilder();
  for (let i = 0; i < num_functions; ++i) {
    builder.addFunction('f' + i, kSig_i_v)
        .addBody(wasmI32Const(i))
        .exportFunc();
  }
  return builder;
}

function check(instance) {
  for (let i = 0; i < num_functions; ++i) {
    const expect_liftoff = i != 0 && i != 2;
    assertEquals(
        expect_liftoff, %IsLiftoffFunction(instance.exports['f' + i]),
        'function ' + i);
  }
}

(function testTierTestingFlag() {
  print(arguments.callee.name);
  const instance = create_builder().instantiate();
  check(instance);
})();


async function testTierTestingFlag() {
  print(arguments.callee.name);
  const instance = await create_builder().asyncInstantiate();
  check(instance);
}

assertPromiseResult(testTierTestingFlag());
