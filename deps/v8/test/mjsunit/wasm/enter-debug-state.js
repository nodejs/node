// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

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

function checkForDebugCode(instance) {
  for (let i = 0; i < num_functions; ++i) {
    // Call the function once because of lazy compilation.
    instance.exports['f' + i]();
    assertTrue(%IsWasmDebugFunction(instance.exports['f' + i]));
  }
}

function check(instance) {
  %WasmEnterDebugging();
  checkForDebugCode(instance);

  for (let i = 0; i < num_functions; ++i) {
    %WasmTierUpFunction(instance.exports['f' + i]);
  }
  checkForDebugCode(instance);
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
