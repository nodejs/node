// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

load('test/mjsunit/wasm/wasm-module-builder.js');

const num_functions = 200;

function create_builder(delta = 0) {
  const builder = new WasmModuleBuilder();
  for (let i = 0; i < num_functions; ++i) {
    builder.addFunction('f' + i, kSig_i_v)
        .addBody(wasmI32Const(i + delta))
        .exportFunc();
  }
  return builder;
}

function checkTieredDown(instance) {
  for (let i = 0; i < num_functions; ++i) {
    assertTrue(%IsLiftoffFunction(instance.exports['f' + i]));
  }
}

function checkTieredUp(instance) {
  // Busy waiting until all functions are tiered up.
  let num_liftoff_functions;
  while (true) {
    num_liftoff_functions = 0;
    for (let i = 0; i < num_functions; ++i) {
      if (%IsLiftoffFunction(instance.exports['f' + i])) {
        num_liftoff_functions++;
      }
    }
    if (num_liftoff_functions == 0) return;
  }
}

function check(instance) {
  %WasmTierDownModule(instance);
  checkTieredDown(instance);

  for (let i = 0; i < num_functions; ++i) {
    %WasmTierUpFunction(instance, i);
  }
  checkTieredDown(instance);

  %WasmTierUpModule(instance);
  checkTieredUp(instance);
}

(function testTierDownToLiftoff() {
  print(arguments.callee.name);
  const instance = create_builder().instantiate();
  check(instance);
})();

// Use slightly different module for this test to avoid sharing native module.
async function testTierDownToLiftoffAsync() {
  print(arguments.callee.name);
  const instance = await create_builder(num_functions).asyncInstantiate();
  check(instance);
}

assertPromiseResult(testTierDownToLiftoffAsync());
