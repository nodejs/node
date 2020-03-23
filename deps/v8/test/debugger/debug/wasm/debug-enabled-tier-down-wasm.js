// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/wasm/wasm-module-builder.js");

const num_functions = 200;

// Create a simple Wasm script.
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
  let num_liftoff_functions = 0;
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

const instance = create_builder().instantiate();
const Debug = new DebugWrapper();
Debug.enable();
checkTieredDown(instance);
const newInstance = create_builder(num_functions*2).instantiate();
checkTieredDown(newInstance);
Debug.disable();
checkTieredUp(instance);
checkTieredUp(newInstance);

// Async.
async function testTierDownToLiftoffAsync() {
  const asyncInstance = await create_builder(num_functions).asyncInstantiate();
  Debug.enable();
  checkTieredDown(asyncInstance);
  const newAsyncInstance = await create_builder(num_functions*3).asyncInstantiate();
  checkTieredDown(newAsyncInstance);
  Debug.disable();
  checkTieredUp(asyncInstance);
  checkTieredUp(newAsyncInstance);
}

assertPromiseResult(testTierDownToLiftoffAsync());
