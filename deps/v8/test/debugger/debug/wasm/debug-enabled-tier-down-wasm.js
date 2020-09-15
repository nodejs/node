// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/wasm/wasm-module-builder.js");

const num_functions = 200;

// Create a simple Wasm module.
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

function waitForTieredUp(instance) {
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

const Debug = new DebugWrapper();

(function testTierDownToLiftoff() {
  // In the 'isolates' test, this test runs in parallel to itself on two
  // isolates. All checks below should still hold.
  const instance = create_builder(0).instantiate();
  Debug.enable();
  checkTieredDown(instance);
  const instance2 = create_builder(1).instantiate();
  checkTieredDown(instance2);
  Debug.disable();
  // Eventually the instances will be completely tiered up again.
  waitForTieredUp(instance);
  waitForTieredUp(instance2);
})();

// Test async compilation.
assertPromiseResult((async function testTierDownToLiftoffAsync() {
  // First test: enable the debugger *after* compiling the module.
  const instance = await create_builder(2).asyncInstantiate();
  Debug.enable();
  checkTieredDown(instance);
  const instance2 = await create_builder(3).asyncInstantiate();
  checkTieredDown(instance2);
  Debug.disable();
  waitForTieredUp(instance);
  waitForTieredUp(instance2);

  // Second test: enable the debugger *while* compiling the module.
  const instancePromise = create_builder(4).asyncInstantiate();
  Debug.enable();
  const instance3 = await instancePromise;
  checkTieredDown(instance3);
  Debug.disable();
  waitForTieredUp(instance3);
})());
