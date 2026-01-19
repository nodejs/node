// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

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

function checkDebugCode(instance) {
  for (let i = 0; i < num_functions; ++i) {
    // Call the function once because of lazy compilation.
    instance.exports['f' + i]();
    assertTrue(%IsWasmDebugFunction(instance.exports['f' + i]));
  }
}

function waitForNoDebugCode(instance) {
  // Busy waiting until all functions left debug mode.
  let num_liftoff_functions = 0;
  while (true) {
    num_liftoff_functions = 0;
    for (let i = 0; i < num_functions; ++i) {
      if (%IsWasmDebugFunction(instance.exports['f' + i])) {
        num_liftoff_functions++;
      }
    }
    if (num_liftoff_functions == 0) return;
  }
}

const Debug = new DebugWrapper();

(function testEnterDebugMode() {
  // In the 'isolates' test, this test runs in parallel to itself on two
  // isolates. All checks below should still hold.
  const instance = create_builder(0).instantiate();
  Debug.enable();
  checkDebugCode(instance);
  const instance2 = create_builder(1).instantiate();
  checkDebugCode(instance2);
  Debug.disable();
  // Eventually the instances will have completely left debug mode again.
  waitForNoDebugCode(instance);
  waitForNoDebugCode(instance2);
})();

// Test async compilation.
assertPromiseResult((async function testEnterDebugModeAsync() {
  // First test: enable the debugger *after* compiling the module.
  const instance = await create_builder(2).asyncInstantiate();
  Debug.enable();
  checkDebugCode(instance);
  const instance2 = await create_builder(3).asyncInstantiate();
  checkDebugCode(instance2);
  Debug.disable();
  waitForNoDebugCode(instance);
  waitForNoDebugCode(instance2);

  // Second test: enable the debugger *while* compiling the module.
  const instancePromise = create_builder(4).asyncInstantiate();
  Debug.enable();
  const instance3 = await instancePromise;
  checkDebugCode(instance3);
  Debug.disable();
  waitForNoDebugCode(instance3);
})());
